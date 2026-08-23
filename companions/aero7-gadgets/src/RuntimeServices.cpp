#include "RuntimeServices.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDebug>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QUrlQuery>
#include <QXmlStreamReader>

namespace {
void enforceReplyLimit(QNetworkReply *reply, qint64 maximumBytes)
{
    QObject::connect(reply, &QNetworkReply::downloadProgress, reply,
                     [reply, maximumBytes](qint64 received, qint64 total) {
        if (received > maximumBytes || total > maximumBytes) {
            reply->setProperty("aero7ResponseTooLarge", true);
            reply->abort();
        }
    });
}
}

RuntimeServices *RuntimeServices::instance()
{
    static RuntimeServices service;
    return &service;
}

RuntimeServices::RuntimeServices(QObject *parent)
    : QObject(parent)
{
    m_network.setTransferTimeout(15000);
    m_network.setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
    m_systemTimer.setInterval(750);
    connect(&m_systemTimer, &QTimer::timeout, this, &RuntimeServices::updateSystem);
    m_systemTimer.start();
    updateSystem();

    m_mediaTimer.setInterval(2000);
    connect(&m_mediaTimer, &QTimer::timeout, this, &RuntimeServices::updateMedia);
    m_mediaTimer.start();
    updateMedia();
}

QString RuntimeServices::cacheFile(const QString &category, const QString &key) const
{
    const QString directory = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation)
        + QStringLiteral("/aero7/gadgets/") + category;
    QDir().mkpath(directory);
    const QString digest = QString::fromLatin1(QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Sha256).toHex());
    return directory + QLatin1Char('/') + digest + QStringLiteral(".json");
}

QJsonObject RuntimeServices::readCache(const QString &category, const QString &key) const
{
    QFile file(cacheFile(category, key));
    if (!file.open(QIODevice::ReadOnly)) return {};
    return QJsonDocument::fromJson(file.readAll()).object();
}

void RuntimeServices::writeCache(const QString &category, const QString &key, const QJsonObject &object) const
{
    QSaveFile file(cacheFile(category, key));
    if (!file.open(QIODevice::WriteOnly)) return;
    file.write(QJsonDocument(object).toJson(QJsonDocument::Compact));
    file.commit();
}

bool RuntimeServices::cacheFresh(const QJsonObject &cache, qint64 maximumAgeSeconds) const
{
    const qint64 saved = cache.value(QStringLiteral("savedAt")).toInteger();
    return saved > 0 && QDateTime::currentSecsSinceEpoch() - saved < maximumAgeSeconds;
}

void RuntimeServices::updateSystem()
{
    QFile stat(QStringLiteral("/proc/stat"));
    double cpu = 0.0;
    if (stat.open(QIODevice::ReadOnly)) {
        const QList<QByteArray> fields = stat.readLine().simplified().split(' ');
        quint64 total = 0;
        for (int i = 1; i < fields.size(); ++i) total += fields.at(i).toULongLong();
        const quint64 idle = (fields.size() > 4 ? fields.at(4).toULongLong() : 0)
            + (fields.size() > 5 ? fields.at(5).toULongLong() : 0);
        if (m_previousCpuTotal && total > m_previousCpuTotal) {
            const quint64 totalDelta = total - m_previousCpuTotal;
            const quint64 idleDelta = idle - m_previousCpuIdle;
            cpu = 100.0 * double(totalDelta - qMin(totalDelta, idleDelta)) / double(totalDelta);
        }
        m_previousCpuTotal = total;
        m_previousCpuIdle = idle;
    }

    QFile memory(QStringLiteral("/proc/meminfo"));
    quint64 totalBytes = 0;
    quint64 availableBytes = 0;
    if (memory.open(QIODevice::ReadOnly)) {
        while (!memory.atEnd()) {
            const QByteArray line = memory.readLine();
            if (line.startsWith("MemTotal:")) totalBytes = line.simplified().split(' ').value(1).toULongLong() * 1024;
            if (line.startsWith("MemAvailable:")) availableBytes = line.simplified().split(' ').value(1).toULongLong() * 1024;
        }
    }
    const quint64 used = totalBytes > availableBytes ? totalBytes - availableBytes : 0;
    const double memoryPercent = totalBytes ? 100.0 * double(used) / double(totalBytes) : 0.0;
    emit systemUpdated(qBound(0.0, cpu, 100.0), qBound(0.0, memoryPercent, 100.0), used, totalBytes);
}

void RuntimeServices::requestCurrency(const QString &base, const QString &target, bool force)
{
    const QString normalizedBase = base.trimmed().toUpper();
    const QString normalizedTarget = target.trimmed().toUpper();
    const QString key = normalizedBase + QLatin1Char('-') + normalizedTarget;
    if (normalizedBase == normalizedTarget) {
        emit currencyUpdated(normalizedBase, normalizedTarget, 1.0, QDate::currentDate().toString(Qt::ISODate), false, {});
        return;
    }
    const QJsonObject cached = readCache(QStringLiteral("currency"), key);
    if (!cached.isEmpty()) {
        emit currencyUpdated(normalizedBase, normalizedTarget, cached.value(QStringLiteral("rate")).toDouble(),
                             cached.value(QStringLiteral("updated")).toString(), !cacheFresh(cached, 12 * 3600), {});
        if (!force && cacheFresh(cached, 12 * 3600)) return;
    }

    QUrl url(QStringLiteral("https://api.frankfurter.app/latest"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("from"), normalizedBase);
    query.addQueryItem(QStringLiteral("to"), normalizedTarget);
    url.setQuery(query);
    QNetworkReply *reply = m_network.get(QNetworkRequest(url));
    enforceReplyLimit(reply, 2 * 1024 * 1024);
    connect(reply, &QNetworkReply::finished, this, [this, reply, normalizedBase, normalizedTarget, key, cached]() {
        const QByteArray bytes = reply->read(2 * 1024 * 1024);
        const QString error = reply->error() == QNetworkReply::NoError ? QString() : reply->errorString();
        reply->deleteLater();
        const QJsonObject root = QJsonDocument::fromJson(bytes).object();
        const double rate = root.value(QStringLiteral("rates")).toObject().value(normalizedTarget).toDouble();
        if (rate > 0.0) {
            const QJsonObject result{{QStringLiteral("rate"), rate},
                                     {QStringLiteral("updated"), root.value(QStringLiteral("date")).toString()},
                                     {QStringLiteral("savedAt"), QDateTime::currentSecsSinceEpoch()}};
            writeCache(QStringLiteral("currency"), key, result);
            emit currencyUpdated(normalizedBase, normalizedTarget, rate,
                                 result.value(QStringLiteral("updated")).toString(), false, {});
        } else if (cached.isEmpty()) {
            qWarning().noquote() << "Aero7 Gadgets: currency update failed:" << (error.isEmpty() ? QStringLiteral("invalid response") : error);
            emit currencyUpdated(normalizedBase, normalizedTarget, 0.0, {}, true,
                                 error.isEmpty() ? QStringLiteral("Rates unavailable") : error);
        }
    });
}

void RuntimeServices::requestWeather(const QString &location, double latitude, double longitude, bool fahrenheit, bool force)
{
    const QString key = QStringLiteral("%1,%2,%3").arg(latitude, 0, 'f', 4).arg(longitude, 0, 'f', 4).arg(fahrenheit);
    const QJsonObject cached = readCache(QStringLiteral("weather"), key);
    if (!cached.isEmpty()) {
        QStringList days;
        QVector<double> temperatures;
        QVector<int> codes;
        for (const QJsonValue value : cached.value(QStringLiteral("forecastDays")).toArray()) days << value.toString();
        for (const QJsonValue value : cached.value(QStringLiteral("forecastTemperatures")).toArray()) temperatures << value.toDouble();
        for (const QJsonValue value : cached.value(QStringLiteral("forecastCodes")).toArray()) codes << value.toInt();
        emit weatherUpdated(location, cached.value(QStringLiteral("temperature")).toDouble(),
                            cached.value(QStringLiteral("code")).toInt(), cached.value(QStringLiteral("updated")).toString(),
                            !cacheFresh(cached, 30 * 60), {}, days, temperatures, codes);
        if (!force && cacheFresh(cached, 30 * 60)) return;
    }

    QUrl url(QStringLiteral("https://api.open-meteo.com/v1/forecast"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("latitude"), QString::number(latitude, 'f', 4));
    query.addQueryItem(QStringLiteral("longitude"), QString::number(longitude, 'f', 4));
    query.addQueryItem(QStringLiteral("current"), QStringLiteral("temperature_2m,weather_code"));
    query.addQueryItem(QStringLiteral("daily"), QStringLiteral("weather_code,temperature_2m_max"));
    query.addQueryItem(QStringLiteral("forecast_days"), QStringLiteral("3"));
    query.addQueryItem(QStringLiteral("temperature_unit"), fahrenheit ? QStringLiteral("fahrenheit") : QStringLiteral("celsius"));
    query.addQueryItem(QStringLiteral("timezone"), QStringLiteral("auto"));
    url.setQuery(query);
    QNetworkReply *reply = m_network.get(QNetworkRequest(url));
    enforceReplyLimit(reply, 2 * 1024 * 1024);
    connect(reply, &QNetworkReply::finished, this, [this, reply, location, key, cached]() {
        const QByteArray bytes = reply->read(2 * 1024 * 1024);
        const QString error = reply->error() == QNetworkReply::NoError ? QString() : reply->errorString();
        reply->deleteLater();
        const QJsonObject root = QJsonDocument::fromJson(bytes).object();
        const QJsonObject current = root.value(QStringLiteral("current")).toObject();
        if (current.contains(QStringLiteral("temperature_2m"))) {
            const QJsonObject daily = root.value(QStringLiteral("daily")).toObject();
            const QJsonArray timeValues = daily.value(QStringLiteral("time")).toArray();
            const QJsonArray temperatureValues = daily.value(QStringLiteral("temperature_2m_max")).toArray();
            const QJsonArray codeValues = daily.value(QStringLiteral("weather_code")).toArray();
            QJsonArray forecastDays;
            QJsonArray forecastTemperatures;
            QJsonArray forecastCodes;
            QStringList dayLabels;
            QVector<double> temperatures;
            QVector<int> codes;
            for (int i = 0; i < qMin(3, timeValues.size()); ++i) {
                const QDate date = QDate::fromString(timeValues.at(i).toString(), Qt::ISODate);
                const QString label = QLocale().dayName(date.dayOfWeek(), QLocale::ShortFormat);
                const double forecastTemperature = i < temperatureValues.size() ? temperatureValues.at(i).toDouble() : 0.0;
                const int forecastCode = i < codeValues.size() ? codeValues.at(i).toInt() : 0;
                forecastDays.append(label); forecastTemperatures.append(forecastTemperature); forecastCodes.append(forecastCode);
                dayLabels << label; temperatures << forecastTemperature; codes << forecastCode;
            }
            const QJsonObject result{{QStringLiteral("temperature"), current.value(QStringLiteral("temperature_2m")).toDouble()},
                                     {QStringLiteral("code"), current.value(QStringLiteral("weather_code")).toInt()},
                                     {QStringLiteral("updated"), current.value(QStringLiteral("time")).toString()},
                                     {QStringLiteral("forecastDays"), forecastDays},
                                     {QStringLiteral("forecastTemperatures"), forecastTemperatures},
                                     {QStringLiteral("forecastCodes"), forecastCodes},
                                     {QStringLiteral("savedAt"), QDateTime::currentSecsSinceEpoch()}};
            writeCache(QStringLiteral("weather"), key, result);
            emit weatherUpdated(location, result.value(QStringLiteral("temperature")).toDouble(),
                                result.value(QStringLiteral("code")).toInt(), result.value(QStringLiteral("updated")).toString(), false, {},
                                dayLabels, temperatures, codes);
        } else if (cached.isEmpty()) {
            qWarning().noquote() << "Aero7 Gadgets: weather update failed:" << (error.isEmpty() ? QStringLiteral("invalid response") : error);
            emit weatherUpdated(location, 0.0, -1, {}, true,
                                error.isEmpty() ? QStringLiteral("Weather unavailable") : error, {}, {}, {});
        }
    });
}

void RuntimeServices::requestFeed(const QString &urlText, bool force)
{
    const QUrl url = QUrl::fromUserInput(urlText);
    if (url.scheme() != QStringLiteral("http") && url.scheme() != QStringLiteral("https")) {
        emit feedUpdated(urlText, {}, {}, QStringLiteral("Only HTTP and HTTPS feeds are allowed"));
        return;
    }
    const QJsonObject cached = readCache(QStringLiteral("feeds"), url.toString());
    if (!cached.isEmpty()) {
        QStringList titles;
        QStringList links;
        for (const auto &value : cached.value(QStringLiteral("titles")).toArray()) titles << value.toString();
        for (const auto &value : cached.value(QStringLiteral("links")).toArray()) links << value.toString();
        emit feedUpdated(url.toString(), titles, links, {});
        if (!force && cacheFresh(cached, 30 * 60)) return;
    }

    QNetworkReply *reply = m_network.get(QNetworkRequest(url));
    enforceReplyLimit(reply, 4 * 1024 * 1024);
    connect(reply, &QNetworkReply::finished, this, [this, reply, url, cached]() {
        const QByteArray bytes = reply->read(4 * 1024 * 1024);
        const QString networkError = reply->error() == QNetworkReply::NoError ? QString() : reply->errorString();
        reply->deleteLater();
        QStringList titles;
        QStringList links;
        QXmlStreamReader xml(bytes);
        while (!xml.atEnd() && titles.size() < 30) {
            xml.readNext();
            if (!xml.isStartElement()) continue;
            const QString element = xml.name().toString().toLower();
            if (element != QStringLiteral("item") && element != QStringLiteral("entry")) continue;
            QString title;
            QString link;
            while (!(xml.isEndElement() && (xml.name().toString().compare(element, Qt::CaseInsensitive) == 0)) && !xml.atEnd()) {
                xml.readNext();
                if (!xml.isStartElement()) continue;
                const QString child = xml.name().toString().toLower();
                if (child == QStringLiteral("title")) title = xml.readElementText(QXmlStreamReader::SkipChildElements).simplified();
                else if (child == QStringLiteral("link")) {
                    link = xml.attributes().value(QStringLiteral("href")).toString();
                    if (link.isEmpty()) link = xml.readElementText(QXmlStreamReader::SkipChildElements).trimmed();
                }
            }
            if (!title.isEmpty()) { titles << title; links << link; }
        }
        if (!titles.isEmpty()) {
            QJsonArray titleArray;
            QJsonArray linkArray;
            for (const auto &title : titles) titleArray.append(title);
            for (const auto &link : links) linkArray.append(link);
            writeCache(QStringLiteral("feeds"), url.toString(),
                       {{QStringLiteral("titles"), titleArray}, {QStringLiteral("links"), linkArray},
                        {QStringLiteral("savedAt"), QDateTime::currentSecsSinceEpoch()}});
            emit feedUpdated(url.toString(), titles, links, {});
        } else if (cached.isEmpty()) {
            qWarning().noquote() << "Aero7 Gadgets: feed update failed for" << url.toString() << ':'
                                 << (networkError.isEmpty() ? xml.errorString() : networkError);
            emit feedUpdated(url.toString(), {}, {}, networkError.isEmpty() ? xml.errorString() : networkError);
        }
    });
}

void RuntimeServices::updateMedia()
{
    auto bus = QDBusConnection::sessionBus();
    auto *interface = bus.interface();
    if (!interface) return;
    const QDBusReply<QStringList> names = interface->registeredServiceNames();
    QString selected;
    QString fallback;
    for (const QString &name : names.value()) {
        if (!name.startsWith(QStringLiteral("org.mpris.MediaPlayer2."))) continue;
        if (fallback.isEmpty()) fallback = name;
        QDBusInterface properties(name, QStringLiteral("/org/mpris/MediaPlayer2"),
                                  QStringLiteral("org.freedesktop.DBus.Properties"), bus);
        const QDBusReply<QVariant> status = properties.call(QStringLiteral("Get"), QStringLiteral("org.mpris.MediaPlayer2.Player"), QStringLiteral("PlaybackStatus"));
        if (status.isValid() && status.value().toString() == QStringLiteral("Playing")) { selected = name; break; }
    }
    if (selected.isEmpty()) selected = fallback;
    m_activePlayer = selected;
    if (selected.isEmpty()) {
        if (!m_lastArtUrl.isEmpty()) {
            m_lastArtUrl.clear();
            emit mediaArtUpdated({}, {});
        }
        emit mediaUpdated(QStringLiteral("No media playing"), {}, {}, {}, false, false);
        return;
    }
    QDBusInterface properties(selected, QStringLiteral("/org/mpris/MediaPlayer2"),
                              QStringLiteral("org.freedesktop.DBus.Properties"), bus);
    const QDBusReply<QVariant> status = properties.call(QStringLiteral("Get"), QStringLiteral("org.mpris.MediaPlayer2.Player"), QStringLiteral("PlaybackStatus"));
    const QDBusReply<QVariant> metadataReply = properties.call(QStringLiteral("Get"), QStringLiteral("org.mpris.MediaPlayer2.Player"), QStringLiteral("Metadata"));
    const QVariantMap metadata = qdbus_cast<QVariantMap>(metadataReply.value());
    const QString artUrl = metadata.value(QStringLiteral("mpris:artUrl")).toString();
    emit mediaUpdated(metadata.value(QStringLiteral("xesam:title"), QStringLiteral("No media playing")).toString(),
                      metadata.value(QStringLiteral("xesam:artist")).toStringList().join(QStringLiteral(", ")),
                      metadata.value(QStringLiteral("xesam:album")).toString(),
                      artUrl,
                      status.value().toString() == QStringLiteral("Playing"), true);
    if (!artUrl.isEmpty() && artUrl != m_lastArtUrl) {
        m_lastArtUrl = artUrl;
        const QUrl url(artUrl);
        if (url.isLocalFile()) {
            QImage image(url.toLocalFile());
            if (!image.isNull()) emit mediaArtUpdated(artUrl, image);
        } else if (url.scheme() == QStringLiteral("http") || url.scheme() == QStringLiteral("https")) {
            QNetworkReply *reply = m_network.get(QNetworkRequest(url));
            enforceReplyLimit(reply, 10 * 1024 * 1024);
            connect(reply, &QNetworkReply::finished, this, [this, reply, artUrl]() {
                const QByteArray bytes = reply->read(10 * 1024 * 1024);
                const QImage image = QImage::fromData(bytes);
                reply->deleteLater();
                if (!image.isNull() && artUrl == m_lastArtUrl) emit mediaArtUpdated(artUrl, image);
            });
        }
    }
}

void RuntimeServices::mediaCommand(const QString &method)
{
    static const QSet<QString> allowed{QStringLiteral("PlayPause"), QStringLiteral("Previous"), QStringLiteral("Next")};
    if (m_activePlayer.isEmpty() || !allowed.contains(method)) return;
    QDBusInterface player(m_activePlayer, QStringLiteral("/org/mpris/MediaPlayer2"),
                          QStringLiteral("org.mpris.MediaPlayer2.Player"), QDBusConnection::sessionBus());
    player.asyncCall(method);
}
