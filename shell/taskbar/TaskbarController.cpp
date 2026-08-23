// SPDX-License-Identifier: MIT
#include "TaskbarController.h"

#include <abstracttasksmodel.h>
#include <tasksmodel.h>
#include <KIO/ApplicationLauncherJob>
#include <KService>
#include <KServiceAction>

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusPendingCall>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUrl>
#include <QXmlStreamReader>

namespace
{
QString configuredPath(const char *environmentName, const QString &fallback)
{
    const auto value = qEnvironmentVariable(environmentName);
    return value.isEmpty() ? fallback : value;
}

KService::Ptr serviceForLauncher(const QUrl &launcher)
{
    QString desktop = launcher.toString();
    if (desktop.startsWith(QStringLiteral("applications:"))) desktop.remove(0, 13);
    if (desktop.endsWith(QStringLiteral(".desktop"))) desktop.chop(8);
    auto service = KService::serviceByDesktopName(desktop);
    if (!service && launcher.isLocalFile()) service = KService::serviceByDesktopPath(launcher.toLocalFile());
    return service;
}
}

TaskbarController::TaskbarController(QObject *parent)
    : QObject(parent)
    , m_tasks(new TaskManager::TasksModel(this))
    , m_pins(configPath(), defaultsPath())
{
    m_tasks->classBegin();
    m_tasks->setFilterByScreen(false);
    m_tasks->setFilterByVirtualDesktop(false);
    m_tasks->setFilterByActivity(false);
    m_tasks->setSeparateLaunchers(false);
    m_tasks->setLaunchInPlace(true);
    m_tasks->setHideActivatedLaunchers(true);
    m_tasks->setSortMode(TaskManager::TasksModel::SortManual);
    m_tasks->setGroupMode(TaskManager::TasksModel::GroupApplications);
    m_tasks->setGroupInline(false);
    m_tasks->setTaskReorderingEnabled(true);
    m_tasks->setLauncherList(m_pins.load());
    m_tasks->componentComplete();

    connect(m_tasks, &TaskManager::TasksModel::countChanged, this, &TaskbarController::taskCountChanged);
    connect(m_tasks, &TaskManager::TasksModel::launcherListChanged, this, [this]() {
        persistPins();
        Q_EMIT launcherCountChanged();
    });

    auto bus = QDBusConnection::sessionBus();
    bus.connect(QStringLiteral("org.kde.KWin"), QStringLiteral("/KWin"), QStringLiteral("org.kde.KWin"),
                QStringLiteral("showingDesktopChanged"), this, SLOT(handleShowingDesktopChanged(bool)));
    QDBusInterface kwin(QStringLiteral("org.kde.KWin"), QStringLiteral("/KWin"), QStringLiteral("org.kde.KWin"));
    m_showingDesktop = kwin.property("showingDesktop").toBool();
}

TaskbarController::~TaskbarController() = default;

QObject *TaskbarController::tasksModel() const
{
    return m_tasks;
}

int TaskbarController::screenCount() const
{
    return m_screenCount;
}

int TaskbarController::taskCount() const
{
    return m_tasks->rowCount();
}

int TaskbarController::launcherCount() const
{
    return m_tasks->launcherList().size();
}

bool TaskbarController::showingDesktop() const
{
    QDBusInterface kwin(QStringLiteral("org.kde.KWin"), QStringLiteral("/KWin"), QStringLiteral("org.kde.KWin"));
    return kwin.isValid() ? kwin.property("showingDesktop").toBool() : m_showingDesktop;
}

void TaskbarController::setScreenCount(int count)
{
    if (m_screenCount == count) {
        return;
    }
    m_screenCount = count;
    Q_EMIT screenCountChanged();
}

void TaskbarController::activate(int row)
{
    const auto index = m_tasks->makeModelIndex(row);
    if (!index.isValid()) {
        return;
    }
    const bool active = index.data(TaskManager::AbstractTasksModel::IsActive).toBool();
    const bool minimizable = index.data(TaskManager::AbstractTasksModel::IsMinimizable).toBool();
    if (active && minimizable) {
        m_tasks->requestToggleMinimized(index);
    } else {
        m_tasks->requestActivate(index);
    }
}

void TaskbarController::activateChild(int row, int childRow)
{
    const auto index = m_tasks->makeModelIndex(row, childRow);
    if (index.isValid()) m_tasks->requestActivate(index);
}

void TaskbarController::launchNewInstance(int row)
{
    const auto index = m_tasks->makeModelIndex(row);
    if (index.isValid()) {
        m_tasks->requestNewInstance(index);
    }
}

void TaskbarController::close(int row)
{
    const auto index = m_tasks->makeModelIndex(row);
    if (index.isValid()) {
        m_tasks->requestClose(index);
    }
}

void TaskbarController::closeChild(int row, int childRow)
{
    const auto index = m_tasks->makeModelIndex(row, childRow);
    if (index.isValid()) m_tasks->requestClose(index);
}

bool TaskbarController::togglePin(int row)
{
    const auto index = m_tasks->makeModelIndex(row);
    if (!index.isValid()) {
        return false;
    }
    const auto launcher = index.data(TaskManager::AbstractTasksModel::LauncherUrlWithoutIcon).toUrl();
    if (!launcher.isValid() || launcher.isEmpty()) {
        return false;
    }
    if (isPinned(launcher)) {
        return m_tasks->requestRemoveLauncher(launcher);
    }
    return m_tasks->requestAddLauncher(launcher);
}

bool TaskbarController::pinLauncher(const QString &launcherValue)
{
    const QUrl launcherUrl(launcherValue);
    if (!launcherUrl.isValid() || launcherUrl.isEmpty()) return false;
    if (isPinned(launcherUrl)) return true;
    return m_tasks->requestAddLauncher(launcherUrl);
}

bool TaskbarController::isPinned(const QUrl &launcherUrl) const
{
    return launcherUrl.isValid() && m_tasks->launcherPosition(launcherUrl) >= 0;
}

void TaskbarController::move(int row, int newPosition)
{
    if (m_tasks->move(row, newPosition)) {
        m_tasks->syncLaunchers();
    }
}

void TaskbarController::publishDelegateGeometry(int row, const QRect &geometry, QObject *delegate)
{
    const auto index = m_tasks->makeModelIndex(row);
    if (index.isValid()) {
        m_tasks->requestPublishDelegateGeometry(index, geometry, delegate);
    }
}

void TaskbarController::toggleStart(const QString &screenName)
{
    QDBusInterface start(QStringLiteral("org.aero7.Start"), QStringLiteral("/Start"), QStringLiteral("org.aero7.Start"));
    start.asyncCall(QStringLiteral("toggleOnScreen"), screenName);
}

void TaskbarController::beginWindowPeek(const QStringList &windowIds)
{
    if (windowIds == m_peekWindows) return;
    m_peekWindows = windowIds;
    QDBusInterface highlight(QStringLiteral("org.kde.KWin.HighlightWindow"),
                             QStringLiteral("/org/kde/KWin/HighlightWindow"),
                             QStringLiteral("org.kde.KWin.HighlightWindow"));
    if (highlight.isValid()) highlight.asyncCall(QStringLiteral("highlightWindows"), windowIds);
}

void TaskbarController::endWindowPeek()
{
    if (m_peekWindows.isEmpty()) return;
    m_peekWindows.clear();
    QDBusInterface highlight(QStringLiteral("org.kde.KWin.HighlightWindow"),
                             QStringLiteral("/org/kde/KWin/HighlightWindow"),
                             QStringLiteral("org.kde.KWin.HighlightWindow"));
    if (highlight.isValid()) highlight.asyncCall(QStringLiteral("highlightWindows"), QStringList{});
}

QVariantList TaskbarController::jumpList(int row) const
{
    const auto index = m_tasks->makeModelIndex(row);
    if (!index.isValid()) return {};
    const auto service = serviceForLauncher(
        index.data(TaskManager::AbstractTasksModel::LauncherUrlWithoutIcon).toUrl());
    if (!service) return {};

    QVariantList result;
    for (const auto &action : service->actions()) {
        if (action.noDisplay() || action.isSeparator()) continue;
        result.append(QVariantMap{{QStringLiteral("kind"), QStringLiteral("action")},
                                  {QStringLiteral("title"), action.text()},
                                  {QStringLiteral("icon"), action.icon()},
                                  {QStringLiteral("action"), action.name()}});
    }

    const auto state = jumpListState();
    const auto applicationState = state.value(service->desktopEntryName()).toMap();
    const auto pinned = applicationState.value(QStringLiteral("pinned")).toStringList();
    for (const auto &url : pinned) {
        const QUrl itemUrl(url);
        result.append(QVariantMap{{QStringLiteral("kind"), QStringLiteral("file")},
                                  {QStringLiteral("title"), itemUrl.fileName()},
                                  {QStringLiteral("icon"), QStringLiteral("emblem-favorite")},
                                  {QStringLiteral("url"), url},
                                  {QStringLiteral("pinned"), true}});
    }

    QFile recent(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
                 + QStringLiteral("/recently-used.xbel"));
    if (!recent.open(QIODevice::ReadOnly)) return result;
    QXmlStreamReader xml(&recent);
    QMimeDatabase mimeDatabase;
    int recentCount = 0;
    while (!xml.atEnd() && recentCount < 6) {
        xml.readNext();
        if (!xml.isStartElement() || xml.name() != QStringLiteral("bookmark")) continue;
        const QUrl url(xml.attributes().value(QStringLiteral("href")).toString());
        if (!url.isValid() || pinned.contains(url.toString())) continue;
        if (url.isLocalFile() && !QFileInfo::exists(url.toLocalFile())) continue;
        const auto mime = mimeDatabase.mimeTypeForUrl(url).name();
        if (!service->mimeTypes().isEmpty() && !service->mimeTypes().contains(mime)) continue;
        result.append(QVariantMap{{QStringLiteral("kind"), QStringLiteral("file")},
                                  {QStringLiteral("title"), url.fileName()},
                                  {QStringLiteral("icon"), mimeDatabase.mimeTypeForName(mime).iconName()},
                                  {QStringLiteral("url"), url.toString()},
                                  {QStringLiteral("pinned"), false}});
        ++recentCount;
    }
    return result;
}

void TaskbarController::beginDesktopPeek()
{
    if (m_desktopPeekActive) return;
    m_desktopPeekRestore = showingDesktop();
    m_desktopPeekActive = true;
    if (!m_desktopPeekRestore) {
        QDBusInterface kwin(QStringLiteral("org.kde.KWin"), QStringLiteral("/KWin"), QStringLiteral("org.kde.KWin"));
        if (kwin.isValid()) kwin.asyncCall(QStringLiteral("showDesktop"), true);
    }
}

void TaskbarController::endDesktopPeek()
{
    if (!m_desktopPeekActive) return;
    const bool restore = m_desktopPeekRestore;
    m_desktopPeekActive = false;
    QDBusInterface kwin(QStringLiteral("org.kde.KWin"), QStringLiteral("/KWin"), QStringLiteral("org.kde.KWin"));
    if (kwin.isValid()) kwin.asyncCall(QStringLiteral("showDesktop"), restore);
}

void TaskbarController::toggleShowingDesktop()
{
    if (m_desktopPeekActive) endDesktopPeek();
    const bool requested = !showingDesktop();
    QDBusInterface kwin(QStringLiteral("org.kde.KWin"), QStringLiteral("/KWin"), QStringLiteral("org.kde.KWin"));
    if (kwin.isValid()) {
        kwin.asyncCall(QStringLiteral("showDesktop"), requested);
        m_showingDesktop = requested;
        Q_EMIT showingDesktopChanged();
    }
}

bool TaskbarController::launchJumpItem(int row, const QVariantMap &item)
{
    const auto index = m_tasks->makeModelIndex(row);
    if (!index.isValid()) return false;
    const auto service = serviceForLauncher(
        index.data(TaskManager::AbstractTasksModel::LauncherUrlWithoutIcon).toUrl());
    if (!service) return false;
    const auto kind = item.value(QStringLiteral("kind")).toString();
    if (kind == QStringLiteral("action")) {
        const auto actionName = item.value(QStringLiteral("action")).toString();
        for (const auto &action : service->actions()) {
            if (action.name() != actionName || action.noDisplay()) continue;
            (new KIO::ApplicationLauncherJob(action, this))->start();
            return true;
        }
    } else if (kind == QStringLiteral("file")) {
        const QUrl url(item.value(QStringLiteral("url")).toString());
        if (!url.isValid()) return false;
        auto *job = new KIO::ApplicationLauncherJob(service, this);
        job->setUrls({url});
        job->start();
        return true;
    }
    return false;
}

bool TaskbarController::toggleJumpPin(int row, const QString &url)
{
    const auto index = m_tasks->makeModelIndex(row);
    if (!index.isValid() || !QUrl(url).isValid()) return false;
    const auto service = serviceForLauncher(
        index.data(TaskManager::AbstractTasksModel::LauncherUrlWithoutIcon).toUrl());
    if (!service) return false;
    auto state = jumpListState();
    auto applicationState = state.value(service->desktopEntryName()).toMap();
    auto pinned = applicationState.value(QStringLiteral("pinned")).toStringList();
    if (pinned.contains(url)) pinned.removeAll(url);
    else pinned.prepend(url);
    while (pinned.size() > 12) pinned.removeLast();
    applicationState.insert(QStringLiteral("pinned"), pinned);
    state.insert(service->desktopEntryName(), applicationState);
    return saveJumpListState(state);
}

QString TaskbarController::dumpState() const
{
    QJsonArray pins;
    for (const auto &pin : m_tasks->launcherList()) {
        pins.append(pin);
    }
    QJsonObject state;
    state.insert(QStringLiteral("schema"), 1);
    state.insert(QStringLiteral("screens"), m_screenCount);
    state.insert(QStringLiteral("tasks"), taskCount());
    state.insert(QStringLiteral("launchers"), launcherCount());
    state.insert(QStringLiteral("pins"), pins);
    state.insert(QStringLiteral("showingDesktop"), showingDesktop());
    state.insert(QStringLiteral("desktopPeek"), m_desktopPeekActive);
    state.insert(QStringLiteral("windowPeekCount"), m_peekWindows.size());
    QJsonArray items;
    for (int row = 0; row < m_tasks->rowCount(); ++row) {
        const auto index = m_tasks->makeModelIndex(row);
        QJsonObject item;
        item.insert(QStringLiteral("row"), row);
        item.insert(QStringLiteral("display"), index.data(Qt::DisplayRole).toString());
        item.insert(QStringLiteral("appId"), index.data(TaskManager::AbstractTasksModel::AppId).toString());
        item.insert(QStringLiteral("launcherUrl"),
                    index.data(TaskManager::AbstractTasksModel::LauncherUrlWithoutIcon).toUrl().toString());
        item.insert(QStringLiteral("window"), index.data(TaskManager::AbstractTasksModel::IsWindow).toBool());
        item.insert(QStringLiteral("launcher"), index.data(TaskManager::AbstractTasksModel::IsLauncher).toBool());
        item.insert(QStringLiteral("active"), index.data(TaskManager::AbstractTasksModel::IsActive).toBool());
        item.insert(QStringLiteral("minimized"), index.data(TaskManager::AbstractTasksModel::IsMinimized).toBool());
        item.insert(QStringLiteral("closable"), index.data(TaskManager::AbstractTasksModel::IsClosable).toBool());
        item.insert(QStringLiteral("children"), index.data(TaskManager::AbstractTasksModel::ChildCount).toInt());
        item.insert(QStringLiteral("jumpListEntries"), jumpList(row).size());
        QJsonArray windowIds;
        for (const auto &windowId : index.data(TaskManager::AbstractTasksModel::WinIdList).toStringList()) {
            windowIds.append(windowId);
        }
        item.insert(QStringLiteral("windowIds"), windowIds);
        items.append(item);
    }
    state.insert(QStringLiteral("items"), items);
    return QString::fromUtf8(QJsonDocument(state).toJson(QJsonDocument::Compact));
}

QString TaskbarController::configPath() const
{
    return configuredPath("AERO7_TASKBAR_CONFIG",
                          QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
                              + QStringLiteral("/aero7-desktop/taskbar.json"));
}

QString TaskbarController::defaultsPath() const
{
    return configuredPath("AERO7_TASKBAR_DEFAULTS", QStringLiteral("/usr/share/aero7-desktop/defaults/taskbar.json"));
}

void TaskbarController::persistPins()
{
    if (!m_pins.save(m_tasks->launcherList())) {
        qWarning("Could not persist Aero7 taskbar pins");
        Q_EMIT operationError(QStringLiteral("Could not save taskbar changes"),
                              QStringLiteral("Aero7 could not write the taskbar configuration. Check that your configuration folder is writable."));
    }
}

QString TaskbarController::jumpListsPath() const
{
    return configuredPath("AERO7_JUMP_LISTS_CONFIG",
                          QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
                              + QStringLiteral("/aero7-desktop/jump-lists.json"));
}

QVariantMap TaskbarController::jumpListState() const
{
    QFile file(jumpListsPath());
    if (!file.open(QIODevice::ReadOnly)) return {};
    const auto document = QJsonDocument::fromJson(file.readAll());
    return document.isObject() ? document.object().toVariantMap() : QVariantMap{};
}

bool TaskbarController::saveJumpListState(const QVariantMap &state) const
{
    const QFileInfo info(jumpListsPath());
    QDir().mkpath(info.absolutePath());
    QSaveFile file(info.absoluteFilePath());
    if (!file.open(QIODevice::WriteOnly)) return false;
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    file.write(QJsonDocument::fromVariant(state).toJson(QJsonDocument::Indented));
    return file.commit();
}

void TaskbarController::handleShowingDesktopChanged(bool showing)
{
    if (m_showingDesktop == showing) {
        return;
    }
    m_showingDesktop = showing;
    Q_EMIT showingDesktopChanged();
}
