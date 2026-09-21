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
#include <cmath>

namespace {
bool finiteNumber(const QJsonValue &value)
{
    return value.isDouble() && std::isfinite(value.toDouble());
}

bool conditionCode(const QJsonValue &value)
{
    if (!finiteNumber(value) || value.toDouble() < 0 || value.toDouble() > 99
        || std::floor(value.toDouble()) != value.toDouble()) return false;
    // Open-Meteo's documented WMO interpretation codes, not every number 0-99.
    // https://open-meteo.com/en/docs#weather_variable_documentation
    switch (value.toInt()) {
    case 0: case 1: case 2: case 3: case 45: case 48:
    case 51: case 53: case 55: case 56: case 57:
    case 61: case 63: case 65: case 66: case 67:
    case 71: case 73: case 75: case 77:
    case 80: case 81: case 82: case 85: case 86:
    case 95: case 96: case 99:
        return true;
    default:
        return false;
    }
}

bool validWeather(const QJsonObject &reading)
{
    return finiteNumber(reading.value(QStringLiteral("temperature")))
        && conditionCode(reading.value(QStringLiteral("code")))
        && QDateTime::fromString(reading.value(QStringLiteral("updated")).toString(), Qt::ISODate).isValid();
}

bool validCurrency(const QJsonObject &reading)
{
    return finiteNumber(reading.value(QStringLiteral("rate")))
        && reading.value(QStringLiteral("rate")).toDouble() > 0
        && QDate::fromString(reading.value(QStringLiteral("updated")).toString(), Qt::ISODate).isValid();
}

bool webUrl(const QUrl &url)
{
    return url.isValid() && !url.host().isEmpty()
        && (url.scheme() == QStringLiteral("https") || url.scheme() == QStringLiteral("http"));
}

bool validFeed(const QJsonObject &reading)
{
    const auto titles = reading.value(QStringLiteral("titles"));
    const auto links = reading.value(QStringLiteral("links"));
    if (!titles.isArray() || !links.isArray()) return false;
    const auto titleArray = titles.toArray();
    const auto linkArray = links.toArray();
    if (titleArray.size() != linkArray.size() || titleArray.size() > 30) return false;
    for (int i = 0; i < titleArray.size(); ++i) {
        if (!titleArray[i].isString() || titleArray[i].toString().trimmed().isEmpty()
            || !linkArray[i].isString()) return false;
        if (!linkArray[i].toString().isEmpty() && !webUrl(QUrl(linkArray[i].toString()))) return false;
    }
    return true;
}

QString parseFeed(const QByteArray &bytes, const QUrl &documentUrl, QStringList &titles, QStringList &links)
{
    QXmlStreamReader xml(bytes);
    QList<QUrl> bases;
    QStringList elements;
    QString root, title, link, text;
    QString field;
    QUrl fieldBase;
    int entryDepth = 0, fieldDepth = 0;
    bool recognized = false;
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isDTD()) return QStringLiteral("Feed document types are not supported");
        if (xml.isStartElement()) {
            const QString name = xml.name().toString();
            const QUrl parentBase = bases.isEmpty() ? documentUrl : bases.last();
            const QString base = xml.attributes().value(QStringLiteral("http://www.w3.org/XML/1998/namespace"), QStringLiteral("base")).toString();
            bases.append(base.isEmpty() ? parentBase : parentBase.resolved(QUrl(base)));
            elements.append(name);
            const int depth = elements.size();
            if (depth > 128) return QStringLiteral("Feed nesting is too deep");
            if (depth == 1) {
                root = name;
                recognized = root == QStringLiteral("rss") || root == QStringLiteral("feed") || root == QStringLiteral("RDF");
                if (!recognized) return QStringLiteral("Not an RSS or Atom feed");
            }
            const bool atomEntry = root == QStringLiteral("feed") && depth == 2 && name == QStringLiteral("entry");
            const bool rssEntry = name == QStringLiteral("item")
                && ((root == QStringLiteral("rss") && depth == 3 && elements.at(1) == QStringLiteral("channel"))
                    || (root == QStringLiteral("RDF") && depth == 2));
            if (atomEntry || rssEntry) {
                entryDepth = depth;
                title.clear(); link.clear();
            } else if (entryDepth && depth == entryDepth + 1) {
                if (name == QStringLiteral("title") || (name == QStringLiteral("link") && root != QStringLiteral("feed"))) {
                    field = name; fieldDepth = depth; fieldBase = bases.last(); text.clear();
                } else if (name == QStringLiteral("link") && root == QStringLiteral("feed")) {
                    const auto attrs = xml.attributes();
                    const QString rel = attrs.value(QStringLiteral("rel")).toString();
                    const QString href = attrs.value(QStringLiteral("href")).toString().trimmed();
                    // RFC 4287: omitted rel means alternate, not self/enclosure.
                    const bool alternate = rel.isEmpty() || rel == QStringLiteral("alternate")
                        || rel == QStringLiteral("http://www.iana.org/assignments/relation/alternate");
                    const QUrl resolved = bases.last().resolved(QUrl(href));
                    if (link.isEmpty() && alternate && !href.isEmpty() && webUrl(resolved)) link = resolved.toString();
                }
            }
        } else if (xml.isCharacters() && fieldDepth) {
            text += xml.text();
        } else if (xml.isEndElement()) {
            const int depth = elements.size();
            if (depth == fieldDepth) {
                if (field == QStringLiteral("title")) title = text.simplified();
                else if (!text.trimmed().isEmpty()) {
                    const QUrl resolved = fieldBase.resolved(QUrl(text.trimmed()));
                    if (webUrl(resolved)) link = resolved.toString();
                }
                fieldDepth = 0; field.clear();
            }
            if (depth == entryDepth) {
                if (!title.isEmpty() && titles.size() < 30) { titles << title; links << link; }
                entryDepth = 0;
            }
            if (!elements.isEmpty()) elements.removeLast();
            if (!bases.isEmpty()) bases.removeLast();
        }
    }
    // Consume the entire bounded document, including everything after item 30.
    if (xml.hasError()) return xml.errorString();
    return recognized ? QString() : QStringLiteral("Empty feed response");
}

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

RuntimeServices::RuntimeServices(QObject *parent, QNetworkAccessManager *network)
    : QObject(parent)
    , m_network(network ? network : new QNetworkAccessManager(this))
{
    m_network->setTransferTimeout(15000);
    m_network->setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
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
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    return saved > 0 && saved <= now && now - saved < maximumAgeSeconds;
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
    QJsonObject cached = readCache(QStringLiteral("currency"), key);
    if (!validCurrency(cached)) cached = {};
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
    QNetworkReply *reply = m_network->get(QNetworkRequest(url));
    m_latestReplies.insert(QStringLiteral("currency:") + key, reply);
    enforceReplyLimit(reply, 2 * 1024 * 1024);
    connect(reply, &QNetworkReply::finished, this, [this, reply, normalizedBase, normalizedTarget, key, cached]() {
        if (!acceptReply(QStringLiteral("currency:") + key, reply)) return;
        const bool oversized = reply->property("aero7ResponseTooLarge").toBool() || reply->bytesAvailable() > 2 * 1024 * 1024;
        const QByteArray bytes = reply->read(2 * 1024 * 1024);
        const QString error = oversized ? QStringLiteral("Response too large")
            : reply->error() == QNetworkReply::NoError ? QString() : reply->errorString();
        reply->deleteLater();
        const QJsonObject root = QJsonDocument::fromJson(bytes).object();
        const QJsonObject result{{QStringLiteral("rate"), root.value(QStringLiteral("rates")).toObject().value(normalizedTarget)},
                                 {QStringLiteral("updated"), root.value(QStringLiteral("date"))},
                                 {QStringLiteral("savedAt"), QDateTime::currentSecsSinceEpoch()}};
        if (error.isEmpty() && validCurrency(result)) {
            writeCache(QStringLiteral("currency"), key, result);
            emit currencyUpdated(normalizedBase, normalizedTarget, result.value(QStringLiteral("rate")).toDouble(),
                                 result.value(QStringLiteral("updated")).toString(), false, {});
        } else {
            qWarning().noquote() << "Aero7 Gadgets: currency update failed:" << (error.isEmpty() ? QStringLiteral("invalid response") : error);
            emit currencyUpdated(normalizedBase, normalizedTarget, cached.value(QStringLiteral("rate")).toDouble(),
                                 cached.value(QStringLiteral("updated")).toString(), true,
                                 error.isEmpty() ? QStringLiteral("Rates unavailable") : error);
        }
    });
}

bool RuntimeServices::acceptReply(const QString &key, QNetworkReply *reply)
{
    // A superseded response must not replace a newer result or its cache.
    if (m_latestReplies.value(key) != reply) {
        reply->deleteLater();
        return false;
    }
    m_latestReplies.remove(key);
    return true;
}

QString RuntimeServices::weatherRequestKey(double latitude, double longitude, bool fahrenheit)
{
    return QStringLiteral("%1,%2,%3").arg(latitude, 0, 'f', 4).arg(longitude, 0, 'f', 4).arg(fahrenheit);
}

void RuntimeServices::publishWeather(const QString &location, const QString &key, const QJsonObject &reading, bool stale, const QString &error)
{
    QStringList days;
    QVector<double> temperatures;
    QVector<int> codes;
    const auto labels = reading.value(QStringLiteral("forecastDays")).toArray();
    const auto values = reading.value(QStringLiteral("forecastTemperatures")).toArray();
    const auto conditions = reading.value(QStringLiteral("forecastCodes")).toArray();
    for (int i = 0; i < qMin(3, labels.size()); ++i) {
        if (i >= values.size() || i >= conditions.size() || labels.at(i).toString().isEmpty()
            || !finiteNumber(values.at(i)) || !conditionCode(conditions.at(i))) continue;
        days << labels.at(i).toString();
        temperatures << values.at(i).toDouble();
        codes << conditions.at(i).toInt();
    }
    emit weatherUpdated(location, reading.value(QStringLiteral("temperature")).toDouble(),
                        reading.value(QStringLiteral("code")).toInt(-1), reading.value(QStringLiteral("updated")).toString(),
                        stale, error, days, temperatures, codes, key);
}

void RuntimeServices::requestWeather(const QString &location, double latitude, double longitude, bool fahrenheit, bool force)
{
    const QString key = weatherRequestKey(latitude, longitude, fahrenheit);
    QJsonObject cached = readCache(QStringLiteral("weather"), key);
    if (!validWeather(cached)) cached = {};
    if (!cached.isEmpty()) {
        publishWeather(location, key, cached, !cacheFresh(cached, 30 * 60), {});
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
    QNetworkReply *reply = m_network->get(QNetworkRequest(url));
    m_latestReplies.insert(QStringLiteral("weather:") + key, reply);
    enforceReplyLimit(reply, 2 * 1024 * 1024);
    connect(reply, &QNetworkReply::finished, this, [this, reply, location, key, cached]() {
        if (!acceptReply(QStringLiteral("weather:") + key, reply)) return;
        const bool oversized = reply->property("aero7ResponseTooLarge").toBool() || reply->bytesAvailable() > 2 * 1024 * 1024;
        const QByteArray bytes = reply->read(2 * 1024 * 1024);
        const QString error = oversized ? QStringLiteral("Response too large")
            : reply->error() == QNetworkReply::NoError ? QString() : reply->errorString();
        reply->deleteLater();
        const QJsonObject root = QJsonDocument::fromJson(bytes).object();
        const QJsonObject current = root.value(QStringLiteral("current")).toObject();
        QJsonObject result{{QStringLiteral("temperature"), current.value(QStringLiteral("temperature_2m"))},
                           {QStringLiteral("code"), current.value(QStringLiteral("weather_code"))},
                           {QStringLiteral("updated"), current.value(QStringLiteral("time"))},
                           {QStringLiteral("savedAt"), QDateTime::currentSecsSinceEpoch()}};
        if (error.isEmpty() && validWeather(result)) {
            const QJsonObject daily = root.value(QStringLiteral("daily")).toObject();
            const QJsonArray timeValues = daily.value(QStringLiteral("time")).toArray();
            const QJsonArray temperatureValues = daily.value(QStringLiteral("temperature_2m_max")).toArray();
            const QJsonArray codeValues = daily.value(QStringLiteral("weather_code")).toArray();
            QJsonArray forecastDays;
            QJsonArray forecastTemperatures;
            QJsonArray forecastCodes;
            for (int i = 0; i < qMin(3, timeValues.size()); ++i) {
                const QDate date = QDate::fromString(timeValues.at(i).toString(), Qt::ISODate);
                if (!date.isValid() || i >= temperatureValues.size() || i >= codeValues.size()
                    || !finiteNumber(temperatureValues.at(i)) || !conditionCode(codeValues.at(i))) continue;
                const QString label = QLocale().dayName(date.dayOfWeek(), QLocale::ShortFormat);
                const double forecastTemperature = temperatureValues.at(i).toDouble();
                const int forecastCode = codeValues.at(i).toInt();
                forecastDays.append(label); forecastTemperatures.append(forecastTemperature); forecastCodes.append(forecastCode);
            }
            result.insert(QStringLiteral("forecastDays"), forecastDays);
            result.insert(QStringLiteral("forecastTemperatures"), forecastTemperatures);
            result.insert(QStringLiteral("forecastCodes"), forecastCodes);
            writeCache(QStringLiteral("weather"), key, result);
            publishWeather(location, key, result, false, {});
        } else {
            qWarning().noquote() << "Aero7 Gadgets: weather update failed:" << (error.isEmpty() ? QStringLiteral("invalid response") : error);
            publishWeather(location, key, cached, true, error.isEmpty() ? QStringLiteral("Weather unavailable") : error);
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
    QJsonObject cached = readCache(QStringLiteral("feeds"), url.toString());
    if (!validFeed(cached)) cached = {};
    if (!cached.isEmpty()) {
        QStringList titles;
        QStringList links;
        for (const auto &value : cached.value(QStringLiteral("titles")).toArray()) titles << value.toString();
        for (const auto &value : cached.value(QStringLiteral("links")).toArray()) links << value.toString();
        emit feedUpdated(url.toString(), titles, links, {}, !cacheFresh(cached, 30 * 60));
        if (!force && cacheFresh(cached, 30 * 60)) return;
    }

    QNetworkReply *reply = m_network->get(QNetworkRequest(url));
    m_latestReplies.insert(QStringLiteral("feed:") + url.toString(), reply);
    enforceReplyLimit(reply, 4 * 1024 * 1024);
    connect(reply, &QNetworkReply::finished, this, [this, reply, url, cached]() {
        if (!acceptReply(QStringLiteral("feed:") + url.toString(), reply)) return;
        const bool oversized = reply->property("aero7ResponseTooLarge").toBool() || reply->bytesAvailable() > 4 * 1024 * 1024;
        const QByteArray bytes = reply->read(4 * 1024 * 1024);
        QString error = oversized ? QStringLiteral("Response too large")
            : reply->error() == QNetworkReply::NoError ? QString() : reply->errorString();
        const QUrl documentUrl = reply->url();
        reply->deleteLater();
        QStringList titles;
        QStringList links;
        if (error.isEmpty()) error = parseFeed(bytes, documentUrl, titles, links);
        if (error.isEmpty()) {
            QJsonArray titleArray;
            QJsonArray linkArray;
            for (const auto &title : titles) titleArray.append(title);
            for (const auto &link : links) linkArray.append(link);
            writeCache(QStringLiteral("feeds"), url.toString(),
                       {{QStringLiteral("titles"), titleArray}, {QStringLiteral("links"), linkArray},
                        {QStringLiteral("savedAt"), QDateTime::currentSecsSinceEpoch()}});
            emit feedUpdated(url.toString(), titles, links, {});
        } else {
            titles.clear(); links.clear();
            for (const auto &value : cached.value(QStringLiteral("titles")).toArray()) titles << value.toString();
            for (const auto &value : cached.value(QStringLiteral("links")).toArray()) links << value.toString();
            qWarning().noquote() << "Aero7 Gadgets: feed update failed for" << url.toString() << ':' << error;
            emit feedUpdated(url.toString(), titles, links, error, !cached.isEmpty());
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
    const bool playerChanged = selected != m_activePlayer;
    m_activePlayer = selected;
    if (selected.isEmpty()) {
        m_latestReplies.remove(QStringLiteral("media-art"));
        if (!m_lastArtUrl.isEmpty()) {
            m_lastArtUrl.clear();
            m_mediaArt = {};
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
    if (playerChanged || artUrl != m_lastArtUrl) {
        // A new track must not display the previous cover while its own art is
        // absent, unreadable or downloading. Invalidate by request identity,
        // not just URL: a later track may reuse an earlier artwork URL.
        m_latestReplies.remove(QStringLiteral("media-art"));
        m_lastArtUrl = artUrl;
        m_mediaArt = {};
        emit mediaArtUpdated(artUrl, {});
        if (artUrl.isEmpty()) return;
        const QUrl url(artUrl);
        if (url.isLocalFile()) {
            QImage image(url.toLocalFile());
            if (!image.isNull()) {
                m_mediaArt = image;
                emit mediaArtUpdated(artUrl, image);
            }
        } else if (url.scheme() == QStringLiteral("http") || url.scheme() == QStringLiteral("https")) {
            QNetworkReply *reply = m_network->get(QNetworkRequest(url));
            m_latestReplies.insert(QStringLiteral("media-art"), reply);
            enforceReplyLimit(reply, 10 * 1024 * 1024);
            connect(reply, &QNetworkReply::finished, this, [this, reply, artUrl]() {
                if (!acceptReply(QStringLiteral("media-art"), reply)) return;
                const QByteArray bytes = reply->read(10 * 1024 * 1024);
                const QImage image = reply->error() == QNetworkReply::NoError
                    ? QImage::fromData(bytes) : QImage();
                reply->deleteLater();
                if (!image.isNull() && artUrl == m_lastArtUrl) {
                    m_mediaArt = image;
                    emit mediaArtUpdated(artUrl, image);
                }
            });
        }
    } else {
        // Gadgets may connect after constructor polling or be added later.
        // Replay the current decoded image without fetching it a second time.
        emit mediaArtUpdated(artUrl, m_mediaArt);
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
