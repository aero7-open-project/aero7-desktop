// SPDX-License-Identifier: MIT
#include "ApplicationsModel.h"

#include <KIO/ApplicationLauncherJob>
#include <KService>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QSet>

#include <algorithm>

namespace
{
QJsonObject readObject(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(file.readAll(), &error);
    return error.error == QJsonParseError::NoError && document.isObject() ? document.object() : QJsonObject{};
}
}

ApplicationsModel::ApplicationsModel(QObject *parent)
    : QAbstractListModel(parent)
{
    loadApplications();
    loadState();
    rebuildVisible();
}

int ApplicationsModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_visible.size();
}

QVariant ApplicationsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_visible.size()) {
        return {};
    }
    const auto &application = m_applications.at(m_visible.at(index.row()));
    switch (role) {
    case StorageIdRole: return application.storageId;
    case NameRole: return application.name;
    case GenericNameRole: return application.genericName;
    case IconNameRole: return application.iconName;
    case CategoriesRole: return application.categories;
    case PinnedRole: return application.pinned;
    case RecentRole: return application.lastUsed > 0;
    default: return {};
    }
}

QHash<int, QByteArray> ApplicationsModel::roleNames() const
{
    return {{StorageIdRole, "storageId"}, {NameRole, "name"}, {GenericNameRole, "genericName"},
            {IconNameRole, "iconName"}, {CategoriesRole, "categories"}, {PinnedRole, "pinned"},
            {RecentRole, "recent"}};
}

QString ApplicationsModel::query() const { return m_query; }

void ApplicationsModel::setQuery(const QString &query)
{
    const auto normalized = query.trimmed();
    if (m_query == normalized) return;
    m_query = normalized;
    rebuildVisible();
    Q_EMIT queryChanged();
}

bool ApplicationsModel::showAll() const { return m_showAll; }

void ApplicationsModel::setShowAll(bool showAll)
{
    if (m_showAll == showAll) return;
    m_showAll = showAll;
    rebuildVisible();
    Q_EMIT showAllChanged();
}

int ApplicationsModel::totalCount() const { return m_applications.size(); }

bool ApplicationsModel::launch(int row)
{
    if (row < 0 || row >= m_visible.size()) return false;
    return launchApplication(m_applications[m_visible.at(row)]);
}

bool ApplicationsModel::launchStorageId(const QString &storageId)
{
    for (auto &application : m_applications) {
        if (application.storageId == storageId) return launchApplication(application);
    }
    return false;
}

bool ApplicationsModel::launchApplication(Application &application)
{
    const auto service = KService::serviceByStorageId(application.storageId);
    if (!service) return false;
    auto *job = new KIO::ApplicationLauncherJob(service, this);
    job->start();
    application.lastUsed = QDateTime::currentMSecsSinceEpoch();
    saveState();
    if (m_query.isEmpty() && !m_showAll) rebuildVisible();
    Q_EMIT applicationLaunched();
    return true;
}

bool ApplicationsModel::togglePin(int row)
{
    if (row < 0 || row >= m_visible.size()) return false;
    m_applications[m_visible.at(row)].pinned = !m_applications[m_visible.at(row)].pinned;
    saveState();
    if (m_query.isEmpty() && !m_showAll) rebuildVisible();
    return true;
}

int ApplicationsModel::rowForStorageId(const QString &storageId) const
{
    for (int row = 0; row < m_visible.size(); ++row) {
        if (m_applications.at(m_visible.at(row)).storageId == storageId) return row;
    }
    return -1;
}

QString ApplicationsModel::dumpState() const
{
    QJsonObject state;
    state.insert(QStringLiteral("schema"), 1);
    state.insert(QStringLiteral("total"), totalCount());
    state.insert(QStringLiteral("visible"), rowCount());
    state.insert(QStringLiteral("query"), m_query);
    state.insert(QStringLiteral("showAll"), m_showAll);
    QJsonArray items;
    for (const auto visibleIndex : m_visible) {
        const auto &application = m_applications.at(visibleIndex);
        QJsonObject item;
        item.insert(QStringLiteral("storageId"), application.storageId);
        item.insert(QStringLiteral("name"), application.name);
        item.insert(QStringLiteral("pinned"), application.pinned);
        item.insert(QStringLiteral("recent"), application.lastUsed > 0);
        items.append(item);
    }
    state.insert(QStringLiteral("items"), items);
    return QString::fromUtf8(QJsonDocument(state).toJson(QJsonDocument::Compact));
}

void ApplicationsModel::loadApplications()
{
    const QSet<QString> hiddenIds = {QStringLiteral("systemsettings.desktop"), QStringLiteral("org.kde.systemsettings.desktop"),
                                     QStringLiteral("linux-controlpanel.desktop"), QStringLiteral("org.kde.dolphin.desktop")};
    for (const auto &service : KService::allServices()) {
        if (!service || service->noDisplay() || !service->showInCurrentDesktop() || !service->showOnCurrentPlatform()) continue;
        if (hiddenIds.contains(service->storageId()) || service->name().compare(QStringLiteral("System Settings"), Qt::CaseInsensitive) == 0) continue;
        Application application;
        application.storageId = service->storageId();
        application.name = service->name();
        application.genericName = service->genericName();
        application.iconName = service->icon();
        application.categories = service->categories();
        if (!application.storageId.isEmpty() && !application.name.isEmpty()) m_applications.append(application);
    }
    std::sort(m_applications.begin(), m_applications.end(), [](const auto &left, const auto &right) {
        return QString::localeAwareCompare(left.name, right.name) < 0;
    });
}

void ApplicationsModel::loadState()
{
    auto state = readObject(configPath());
    if (state.isEmpty()) state = readObject(defaultsPath());
    QSet<QString> pins;
    for (const auto &entry : state.value(QStringLiteral("pins")).toArray()) {
        auto storageId = entry.toString();
        if (storageId == QStringLiteral("linux-controlpanel.desktop"))
            storageId = QStringLiteral("org.aero7.controlpanel.desktop");
        pins.insert(storageId);
    }
    QHash<QString, qint64> recent;
    for (const auto &entry : state.value(QStringLiteral("recent")).toArray()) {
        const auto object = entry.toObject();
        recent.insert(object.value(QStringLiteral("storageId")).toString(),
                      static_cast<qint64>(object.value(QStringLiteral("lastUsed")).toDouble()));
    }
    for (auto &application : m_applications) {
        application.pinned = pins.contains(application.storageId);
        application.lastUsed = recent.value(application.storageId);
    }
}

void ApplicationsModel::saveState() const
{
    QDir().mkpath(QFileInfo(configPath()).absolutePath());
    QJsonArray pins;
    QList<const Application *> recent;
    for (const auto &application : m_applications) {
        if (application.pinned) pins.append(application.storageId);
        if (application.lastUsed > 0) recent.append(&application);
    }
    std::sort(recent.begin(), recent.end(), [](const auto *left, const auto *right) { return left->lastUsed > right->lastUsed; });
    QJsonArray recentValues;
    for (qsizetype index = 0; index < std::min<qsizetype>(20, recent.size()); ++index) {
        recentValues.append(QJsonObject{{QStringLiteral("storageId"), recent.at(index)->storageId},
                                        {QStringLiteral("lastUsed"), static_cast<double>(recent.at(index)->lastUsed)}});
    }
    QSaveFile file(configPath());
    if (!file.open(QIODevice::WriteOnly)) return;
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    file.write(QJsonDocument(QJsonObject{{QStringLiteral("schema"), 1}, {QStringLiteral("pins"), pins},
                                         {QStringLiteral("recent"), recentValues}}).toJson(QJsonDocument::Indented));
    if (!file.commit()) qWarning("Could not commit Aero7 Start menu state");
}

void ApplicationsModel::rebuildVisible()
{
    beginResetModel();
    m_visible.clear();
    for (int index = 0; index < m_applications.size(); ++index) {
        const auto &application = m_applications.at(index);
        const bool matches = m_query.isEmpty()
            || application.name.contains(m_query, Qt::CaseInsensitive)
            || application.genericName.contains(m_query, Qt::CaseInsensitive)
            || application.storageId.contains(m_query, Qt::CaseInsensitive);
        if (!matches) continue;
        if (!m_query.isEmpty() || m_showAll || application.pinned || application.lastUsed > 0) m_visible.append(index);
    }
    if (m_query.isEmpty() && !m_showAll) {
        std::stable_sort(m_visible.begin(), m_visible.end(), [this](int left, int right) {
            const auto &a = m_applications.at(left);
            const auto &b = m_applications.at(right);
            if (a.pinned != b.pinned) return a.pinned;
            if (!a.pinned && a.lastUsed != b.lastUsed) return a.lastUsed > b.lastUsed;
            return QString::localeAwareCompare(a.name, b.name) < 0;
        });
    }
    endResetModel();
}

QString ApplicationsModel::configPath() const
{
    const auto override = qEnvironmentVariable("AERO7_START_CONFIG");
    return override.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
            + QStringLiteral("/aero7-desktop/start.json") : override;
}

QString ApplicationsModel::defaultsPath() const
{
    const auto override = qEnvironmentVariable("AERO7_START_DEFAULTS");
    return override.isEmpty() ? QStringLiteral("/usr/share/aero7-desktop/defaults/start.json") : override;
}
