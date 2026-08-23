// SPDX-License-Identifier: MIT
#include "DesktopController.h"

#include <KIO/CopyJob>
#include <KIO/OpenUrlJob>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMimeDatabase>
#include <QProcess>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUuid>

DesktopItemsModel::DesktopItemsModel(QObject *parent)
    : QAbstractListModel(parent)
{
    QDir().mkpath(desktopPath());
    m_watcher.addPath(desktopPath());
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, &DesktopItemsModel::reload);
    reload();
}

int DesktopItemsModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

QVariant DesktopItemsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size()) return {};
    const auto row = m_rows.at(index.row()).toMap();
    switch (role) {
    case NameRole: return row.value(QStringLiteral("name"));
    case UrlRole: return row.value(QStringLiteral("url"));
    case IconRole: return row.value(QStringLiteral("icon"));
    case KindRole: return row.value(QStringLiteral("kind"));
    default: return {};
    }
}

QHash<int, QByteArray> DesktopItemsModel::roleNames() const
{
    return {{NameRole, "name"}, {UrlRole, "url"}, {IconRole, "icon"}, {KindRole, "kind"}};
}

QString DesktopItemsModel::desktopPath() const
{
    const auto overridePath = qEnvironmentVariable("AERO7_DESKTOP_PATH");
    if (!overridePath.isEmpty()) return overridePath;
    return QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
}

void DesktopItemsModel::reload()
{
    QVariantList rows{
        QVariantMap{{QStringLiteral("name"), QStringLiteral("Computer")},
                    {QStringLiteral("url"), QStringLiteral("file:///")},
                    {QStringLiteral("icon"), QStringLiteral("computer")},
                    {QStringLiteral("kind"), QStringLiteral("system")}},
        QVariantMap{{QStringLiteral("name"), QStringLiteral("Recycle Bin")},
                    {QStringLiteral("url"), QStringLiteral("trash:/")},
                    {QStringLiteral("icon"), QStringLiteral("user-trash")},
                    {QStringLiteral("kind"), QStringLiteral("system")}},
    };
    QDir directory(desktopPath());
    QMimeDatabase mimeDatabase;
    const auto entries = directory.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot,
                                                  QDir::DirsFirst | QDir::Name | QDir::IgnoreCase);
    for (const auto &entry : entries) {
        const auto mime = mimeDatabase.mimeTypeForFile(entry);
        rows.append(QVariantMap{{QStringLiteral("name"), entry.completeBaseName()},
                                {QStringLiteral("url"), QUrl::fromLocalFile(entry.absoluteFilePath()).toString()},
                                {QStringLiteral("icon"), entry.isDir() ? QStringLiteral("folder") : mime.iconName()},
                                {QStringLiteral("kind"), entry.isDir() ? QStringLiteral("folder") : QStringLiteral("file")}});
    }
    beginResetModel();
    m_rows = rows;
    endResetModel();
}

DesktopController::DesktopController(QObject *parent)
    : QObject(parent)
{
    m_configPath = qEnvironmentVariable("AERO7_DESKTOP_CONFIG");
    if (m_configPath.isEmpty()) {
        m_configPath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
            + QStringLiteral("/aero7-desktop/desktop.json");
    }
    const auto wallpaperOverride = qEnvironmentVariable("AERO7_WALLPAPER");
    if (!wallpaperOverride.isEmpty()) m_wallpaper = wallpaperOverride;
    QFile config(m_configPath);
    if (config.open(QIODevice::ReadOnly)) {
        const auto object = QJsonDocument::fromJson(config.readAll()).object();
        if (m_wallpaper.isEmpty()) m_wallpaper = object.value(QStringLiteral("wallpaper")).toString();
        for (const auto &value : object.value(QStringLiteral("gadgets")).toArray()) {
            const auto gadget = value.toObject();
            const auto type = gadget.value(QStringLiteral("type")).toString();
            if (!QStringList{QStringLiteral("clock"), QStringLiteral("cpu"), QStringLiteral("notes")}.contains(type))
                continue;
            m_gadgets.append(gadget.toVariantMap());
        }
    }
    if (m_wallpaper.isEmpty()) {
        m_wallpaper = QStringLiteral("/usr/share/aero7-desktop/wallpapers/aero7-default.svg");
    }
    m_cpuTimer.setInterval(1000);
    connect(&m_cpuTimer, &QTimer::timeout, this, &DesktopController::sampleCpuUsage);
    sampleCpuUsage();
    m_cpuTimer.start();
}

QObject *DesktopController::itemsModel() { return &m_items; }
QString DesktopController::wallpaper() const { return m_wallpaper; }
QVariantList DesktopController::gadgets() const { return m_gadgets; }
double DesktopController::cpuUsage() const { return m_cpuUsage; }
int DesktopController::screenCount() const { return m_screenCount; }
void DesktopController::setScreenCount(int count)
{
    if (m_screenCount == count) return;
    m_screenCount = count;
    Q_EMIT screenCountChanged();
}

void DesktopController::refresh() { m_items.reload(); }

bool DesktopController::open(const QString &urlValue)
{
    const QUrl url(urlValue);
    if (!url.isValid()) return false;
    auto *job = new KIO::OpenUrlJob(url, this);
    connect(job, &KJob::result, this, [this, job]() {
        if (job->error()) Q_EMIT operationError(QStringLiteral("Could not open item"), job->errorString());
    });
    job->start();
    return true;
}

bool DesktopController::createFolder(const QString &name)
{
    const QString safeName = name.trimmed();
    if (safeName.isEmpty() || safeName.contains(QLatin1Char('/'))) return false;
    if (!QDir(m_items.desktopPath()).mkdir(safeName)) {
        Q_EMIT operationError(QStringLiteral("Could not create folder"),
                              QStringLiteral("A file with that name may already exist."));
        return false;
    }
    m_items.reload();
    return true;
}

bool DesktopController::createFile(const QString &name)
{
    const QString safeName = name.trimmed();
    if (safeName.isEmpty() || safeName.contains(QLatin1Char('/'))) return false;
    QFile file(QDir(m_items.desktopPath()).filePath(safeName));
    if (!file.open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
        Q_EMIT operationError(QStringLiteral("Could not create file"),
                              QStringLiteral("A file with that name may already exist."));
        return false;
    }
    file.close();
    m_items.reload();
    return true;
}

bool DesktopController::copyToDesktop(const QStringList &urlValues)
{
    QList<QUrl> urls;
    for (const auto &value : urlValues) {
        const QUrl url(value);
        if (url.isValid()) urls.append(url);
    }
    if (urls.isEmpty()) return false;
    auto *job = KIO::copy(urls, QUrl::fromLocalFile(m_items.desktopPath()), KIO::HideProgressInfo);
    connect(job, &KJob::result, this, [this, job]() {
        if (job->error()) Q_EMIT operationError(QStringLiteral("Could not copy to Desktop"), job->errorString());
        m_items.reload();
    });
    return true;
}

bool DesktopController::renameItem(const QString &urlValue, const QString &newName)
{
    const QUrl url(urlValue);
    if (!url.isLocalFile() || newName.trimmed().isEmpty() || newName.contains(QLatin1Char('/'))) return false;
    const QFileInfo info(url.toLocalFile());
    if (info.absolutePath() != QDir(m_items.desktopPath()).absolutePath()) return false;
    const bool renamed = QFile::rename(info.absoluteFilePath(), info.dir().filePath(newName.trimmed()));
    if (!renamed) Q_EMIT operationError(QStringLiteral("Could not rename item"), QStringLiteral("Check the name and permissions."));
    m_items.reload();
    return renamed;
}

bool DesktopController::moveToTrash(const QString &urlValue)
{
    const QUrl url(urlValue);
    if (!url.isLocalFile()) return false;
    auto *job = KIO::trash({url}, KIO::HideProgressInfo);
    connect(job, &KJob::result, this, [this, job]() {
        if (job->error()) Q_EMIT operationError(QStringLiteral("Could not move item to Recycle Bin"), job->errorString());
        m_items.reload();
    });
    return true;
}

bool DesktopController::setWallpaper(const QString &urlValue)
{
    const QUrl url(urlValue);
    const QString path = url.isLocalFile() ? url.toLocalFile() : urlValue;
    const QFileInfo info(path);
    QMimeDatabase mimeDatabase;
    if (!info.isFile() || !info.isReadable()
        || !mimeDatabase.mimeTypeForFile(info).name().startsWith(QStringLiteral("image/"))) {
        Q_EMIT operationError(QStringLiteral("Could not set desktop background"),
                              QStringLiteral("Choose a readable local image file."));
        return false;
    }
    const auto previous = m_wallpaper;
    m_wallpaper = info.absoluteFilePath();
    if (!saveConfiguration()) {
        m_wallpaper = previous;
        Q_EMIT operationError(QStringLiteral("Could not save desktop background"),
                              QStringLiteral("The Aero7 configuration folder is not writable."));
        return false;
    }
    Q_EMIT wallpaperChanged();
    return true;
}

bool DesktopController::saveConfiguration()
{
    if (!QDir().mkpath(QFileInfo(m_configPath).absolutePath())) return false;
    QSaveFile config(m_configPath);
    if (!config.open(QIODevice::WriteOnly)) return false;
    config.write(QJsonDocument(QJsonObject{
        {QStringLiteral("schema"), 2},
        {QStringLiteral("wallpaper"), m_wallpaper},
        {QStringLiteral("gadgets"), QJsonArray::fromVariantList(m_gadgets)},
    }).toJson(QJsonDocument::Indented));
    return config.commit();
}

QString DesktopController::addGadget(const QString &type, const QString &screenName)
{
    if (!QStringList{QStringLiteral("clock"), QStringLiteral("cpu"), QStringLiteral("notes")}.contains(type))
        return {};
    const auto id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const int offset = (m_gadgets.size() % 6) * 24;
    m_gadgets.append(QVariantMap{
        {QStringLiteral("id"), id},
        {QStringLiteral("type"), type},
        {QStringLiteral("screen"), screenName},
        {QStringLiteral("x"), 36 + offset},
        {QStringLiteral("y"), 36 + offset},
        {QStringLiteral("note"), type == QStringLiteral("notes") ? QStringLiteral("Type a note here") : QString{}},
    });
    if (!saveConfiguration()) {
        m_gadgets.removeLast();
        Q_EMIT operationError(QStringLiteral("Could not add gadget"),
                              QStringLiteral("The Aero7 desktop configuration is not writable."));
        return {};
    }
    Q_EMIT gadgetsChanged();
    return id;
}

bool DesktopController::removeGadget(const QString &id)
{
    for (qsizetype index = 0; index < m_gadgets.size(); ++index) {
        if (m_gadgets.at(index).toMap().value(QStringLiteral("id")).toString() != id) continue;
        const auto removed = m_gadgets.takeAt(index);
        if (!saveConfiguration()) {
            m_gadgets.insert(index, removed);
            Q_EMIT operationError(QStringLiteral("Could not remove gadget"),
                                  QStringLiteral("The Aero7 desktop configuration is not writable."));
            return false;
        }
        Q_EMIT gadgetsChanged();
        return true;
    }
    return false;
}

bool DesktopController::setGadgetPosition(const QString &id, int x, int y, const QString &screenName)
{
    for (qsizetype index = 0; index < m_gadgets.size(); ++index) {
        auto gadget = m_gadgets.at(index).toMap();
        if (gadget.value(QStringLiteral("id")).toString() != id) continue;
        const auto previous = gadget;
        gadget.insert(QStringLiteral("x"), qBound(0, x, 16384));
        gadget.insert(QStringLiteral("y"), qBound(0, y, 16384));
        gadget.insert(QStringLiteral("screen"), screenName);
        m_gadgets[index] = gadget;
        if (!saveConfiguration()) {
            m_gadgets[index] = previous;
            return false;
        }
        Q_EMIT gadgetsChanged();
        return true;
    }
    return false;
}

bool DesktopController::setGadgetNote(const QString &id, const QString &note)
{
    for (qsizetype index = 0; index < m_gadgets.size(); ++index) {
        auto gadget = m_gadgets.at(index).toMap();
        if (gadget.value(QStringLiteral("id")).toString() != id
            || gadget.value(QStringLiteral("type")).toString() != QStringLiteral("notes")) continue;
        const auto previous = gadget;
        gadget.insert(QStringLiteral("note"), note.left(10000));
        m_gadgets[index] = gadget;
        if (!saveConfiguration()) {
            m_gadgets[index] = previous;
            return false;
        }
        Q_EMIT gadgetsChanged();
        return true;
    }
    return false;
}

void DesktopController::sampleCpuUsage()
{
    QFile statistics(QStringLiteral("/proc/stat"));
    if (!statistics.open(QIODevice::ReadOnly)) return;
    const auto fields = statistics.readLine().simplified().split(' ');
    if (fields.size() < 5 || fields.first() != QByteArrayLiteral("cpu")) return;
    quint64 total = 0;
    for (qsizetype index = 1; index < fields.size(); ++index) total += fields.at(index).toULongLong();
    const quint64 idle = fields.at(4).toULongLong() + (fields.size() > 5 ? fields.at(5).toULongLong() : 0);
    if (m_previousCpuTotal > 0 && total > m_previousCpuTotal) {
        const auto totalDelta = total - m_previousCpuTotal;
        const auto idleDelta = idle - m_previousCpuIdle;
        m_cpuUsage = qBound(0.0, 100.0 * (1.0 - static_cast<double>(idleDelta) / totalDelta), 100.0);
        Q_EMIT cpuUsageChanged();
    }
    m_previousCpuTotal = total;
    m_previousCpuIdle = idle;
}

void DesktopController::openControlPanel(const QString &setting)
{
    QProcess::startDetached(QStringLiteral("aero7-control-panel"), {QStringLiteral("--setting"), setting});
}

QString DesktopController::dumpState() const
{
    return QString::fromUtf8(QJsonDocument(QJsonObject{
        {QStringLiteral("schema"), 1},
        {QStringLiteral("screens"), m_screenCount},
        {QStringLiteral("items"), m_items.rowCount()},
        {QStringLiteral("desktopPath"), m_items.desktopPath()},
        {QStringLiteral("wallpaper"), m_wallpaper},
        {QStringLiteral("gadgets"), QJsonArray::fromVariantList(m_gadgets)},
        {QStringLiteral("cpuUsage"), m_cpuUsage},
    }).toJson(QJsonDocument::Compact));
}
