/*
    SPDX-FileCopyrightText: 2026 Aero7 Open Project
    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#include "aero7tasksmodel.h"

#include <KIO/ApplicationLauncherJob>
#include <KService>
#include <KServiceAction>

#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <utility>

namespace {
constexpr auto InternetExplorerDesktopId = "aero7-internet-explorer.desktop";

QString launcherForDesktopId(const QString &desktopId)
{
    return QStringLiteral("applications:") + desktopId;
}
}

Aero7TasksModel::Aero7TasksModel(QObject *parent)
    : TaskManager::TasksModel(parent)
{
    connect(this, &TaskManager::TasksModel::launcherListChanged, this, [this] {
        if (m_applyingLaunchers) {
            return;
        }
        QStringList canonical;
        for (const QString &launcher : TaskManager::TasksModel::launcherList()) {
            canonical.append(toShellLauncher(launcher));
        }
        if (canonical != m_shellLaunchers) {
            m_shellLaunchers = canonical;
            saveShellLaunchers();
            Q_EMIT shellLauncherListChanged();
        }
    });
    QSettings savedState(taskbarStatePath(), QSettings::IniFormat);
    m_hasSavedLaunchers = savedState.contains(QStringLiteral("Pinned/Launchers"));
    if (m_hasSavedLaunchers) {
        m_shellLaunchers = savedState.value(QStringLiteral("Pinned/Launchers")).toStringList();
        applyShellLaunchers();
    }
    m_taskOrder = savedState.value(QStringLiteral("Layout/TaskOrder")).toStringList();
    m_orderRestoreTimer.setSingleShot(true);
    m_orderRestoreTimer.setInterval(250);
    connect(&m_orderRestoreTimer, &QTimer::timeout, this, &Aero7TasksModel::restoreTaskOrder);
    connect(this, &QAbstractItemModel::rowsInserted, this,
            [this] { if (!m_restoringTaskOrder && !m_taskOrder.isEmpty()) m_orderRestoreTimer.start(); });
    connect(this, &QAbstractItemModel::modelReset, this,
            [this] { if (!m_taskOrder.isEmpty()) m_orderRestoreTimer.start(); });
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this] {
        refreshWatchPaths();
        refreshBackend();
    });
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] {
        refreshWatchPaths();
        refreshBackend();
    });
    refreshWatchPaths();
    refreshBackend();
}

QString Aero7TasksModel::configPath()
{
    const QString testPath = qEnvironmentVariable("AERO7_IE_TASKBAR_CONFIG");
    if (!testPath.isEmpty()) {
        return testPath;
    }
    return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
        + QStringLiteral("/aero7/internet-explorer.conf");
}

QString Aero7TasksModel::taskbarStatePath()
{
    const QString testPath = qEnvironmentVariable("AERO7_TASKBAR_STATE_CONFIG");
    if (!testPath.isEmpty()) {
        return testPath;
    }
    return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
        + QStringLiteral("/aero7/taskbar-state.ini");
}

void Aero7TasksModel::saveShellLaunchers()
{
    const QString path = taskbarStatePath();
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSettings state(path, QSettings::IniFormat);
    state.setValue(QStringLiteral("Pinned/Launchers"), m_shellLaunchers);
    state.sync();
    m_hasSavedLaunchers = state.status() == QSettings::NoError;
}

QString Aero7TasksModel::taskKeyAt(int row) const
{
    const QModelIndex task = index(row, 0);
    if (!task.isValid()) {
        return {};
    }
    const QUrl launcher = data(task, TaskManager::AbstractTasksModel::LauncherUrlWithoutIcon).toUrl();
    if (launcher.isValid() && !launcher.isEmpty()) {
        return QStringLiteral("launcher:") + launcher.toString(QUrl::RemoveQuery);
    }
    const QString appId = data(task, TaskManager::AbstractTasksModel::AppId).toString();
    return appId.isEmpty() ? QString() : QStringLiteral("app:") + appId;
}

bool Aero7TasksModel::isPinnedTaskAt(int row) const
{
    const QModelIndex task = index(row, 0);
    if (!task.isValid()) {
        return false;
    }
    const QUrl launcher = data(task, TaskManager::AbstractTasksModel::LauncherUrlWithoutIcon).toUrl();
    return !launcher.isEmpty() && launcherPosition(launcher) >= 0;
}

int Aero7TasksModel::pinnedTaskCount() const
{
    int count = 0;
    while (count < rowCount() && isPinnedTaskAt(count)) {
        ++count;
    }
    return count;
}

void Aero7TasksModel::saveTaskOrder()
{
    QStringList order;
    // The launcher list is the single source of truth for pinned buttons.
    // Only remember the relative order of unpinned running applications.
    for (int row = pinnedTaskCount(); row < rowCount(); ++row) {
        const QString key = taskKeyAt(row);
        if (!key.isEmpty() && !order.contains(key)) {
            order.append(key);
        }
    }
    m_taskOrder = order;
    const QString path = taskbarStatePath();
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSettings state(path, QSettings::IniFormat);
    state.setValue(QStringLiteral("Layout/TaskOrder"), m_taskOrder);
    state.sync();
}

bool Aero7TasksModel::move(int row, int newPos, const QModelIndex &parent)
{
    const bool moved = TaskManager::TasksModel::move(row, newPos, parent);
    if (moved && !m_restoringTaskOrder) {
        // KDE defers saving moved launchers until syncLaunchers().  Commit it
        // before persisting window order so a crash cannot restore two
        // conflicting arrangements.  The QML drag-end sync remains harmless.
        m_orderRestoreTimer.stop();
        TaskManager::TasksModel::syncLaunchers();
        saveTaskOrder();
    }
    return moved;
}

void Aero7TasksModel::restoreTaskOrder()
{
    if (m_taskOrder.isEmpty() || sortMode() != TaskManager::TasksModel::SortManual) {
        return;
    }
    m_restoringTaskOrder = true;
    int destination = pinnedTaskCount();
    for (const QString &wanted : std::as_const(m_taskOrder)) {
        for (int row = destination; row < rowCount(); ++row) {
            if (isPinnedTaskAt(row) || taskKeyAt(row) != wanted) {
                continue;
            }
            if (row == destination || TaskManager::TasksModel::move(row, destination)) {
                ++destination;
            }
            break;
        }
    }
    m_restoringTaskOrder = false;
}

QString Aero7TasksModel::policyPath()
{
    const QString testPath = qEnvironmentVariable("AERO7_IE_TASKBAR_POLICY");
    if (!testPath.isEmpty()) {
        return testPath;
    }
    return QStringLiteral("/etc/aero7/internet-explorer.conf");
}

bool Aero7TasksModel::isSafeDesktopId(const QString &desktopId)
{
    static const QRegularExpression safe(
        QStringLiteral(R"(^[A-Za-z0-9][A-Za-z0-9._-]*\.desktop$)"));
    return desktopId.size() <= 255
        && desktopId != QLatin1String(InternetExplorerDesktopId)
        && safe.match(desktopId).hasMatch();
}

QString Aero7TasksModel::effectiveBackend() const
{
    QSettings policy(policyPath(), QSettings::IniFormat);
    const QString enforced = policy.value(QStringLiteral("Policy/Backend"))
                                 .toString().trimmed();
    if (isSafeDesktopId(enforced)) {
        return enforced;
    }
    QSettings user(configPath(), QSettings::IniFormat);
    const QString selected = user.value(QStringLiteral("Browser/Backend"))
                                 .toString().trimmed();
    return isSafeDesktopId(selected) ? selected : QString();
}

QStringList Aero7TasksModel::shellLauncherList() const
{
    return m_shellLaunchers;
}

void Aero7TasksModel::setShellLauncherList(const QStringList &launchers)
{
    if (m_shellLaunchers == launchers) {
        if (!m_hasSavedLaunchers) {
            saveShellLaunchers();
        }
        return;
    }
    m_shellLaunchers = launchers;
    applyShellLaunchers();
    saveShellLaunchers();
    Q_EMIT shellLauncherListChanged();
}

void Aero7TasksModel::restoreOrAdoptLaunchers(const QStringList &panelLaunchers)
{
    if (m_hasSavedLaunchers) {
        // A secondary panel may still have a stale per-applet copy. The
        // shared, crash-safe list wins and is copied back by the QML caller.
        Q_EMIT shellLauncherListChanged();
        return;
    }
    setShellLauncherList(panelLaunchers);
}

QString Aero7TasksModel::internetExplorerBackend() const
{
    return m_backendDesktopId;
}

QString Aero7TasksModel::toBackendLauncher(const QString &launcher) const
{
    if (m_backendDesktopId.isEmpty()) {
        return launcher;
    }
    QString translated = launcher;
    translated.replace(launcherForDesktopId(QString::fromLatin1(InternetExplorerDesktopId)),
                       launcherForDesktopId(m_backendDesktopId));
    return translated;
}

QString Aero7TasksModel::toShellLauncher(const QString &launcher) const
{
    if (m_backendDesktopId.isEmpty()) {
        return launcher;
    }
    QString translated = launcher;
    translated.replace(launcherForDesktopId(m_backendDesktopId),
                       launcherForDesktopId(QString::fromLatin1(InternetExplorerDesktopId)));
    return translated;
}

QUrl Aero7TasksModel::toBackendLauncher(const QUrl &launcher) const
{
    return QUrl(toBackendLauncher(launcher.toString()));
}

QUrl Aero7TasksModel::toShellLauncher(const QUrl &launcher) const
{
    return QUrl(toShellLauncher(launcher.toString()));
}

void Aero7TasksModel::applyShellLaunchers()
{
    QStringList translated;
    translated.reserve(m_shellLaunchers.size());
    for (const QString &launcher : m_shellLaunchers) {
        translated.append(toBackendLauncher(launcher));
    }
    m_applyingLaunchers = true;
    TaskManager::TasksModel::setLauncherList(translated);
    m_applyingLaunchers = false;
}

void Aero7TasksModel::refreshWatchPaths()
{
    QStringList wanted;
    const QString userConfig = configPath();
    const QString userDirectory = QFileInfo(userConfig).absolutePath();
    if (QFileInfo::exists(userDirectory)) {
        wanted.append(userDirectory);
    }
    if (QFileInfo::exists(userConfig)) {
        wanted.append(userConfig);
    }
    if (QFileInfo::exists(policyPath())) {
        wanted.append(policyPath());
    }
    for (const QString &path : m_watcher.files() + m_watcher.directories()) {
        if (!wanted.contains(path)) {
            m_watcher.removePath(path);
        }
    }
    for (const QString &path : wanted) {
        if (!m_watcher.files().contains(path)
            && !m_watcher.directories().contains(path)) {
            m_watcher.addPath(path);
        }
    }
}

void Aero7TasksModel::refreshBackend()
{
    const QString selected = effectiveBackend();
    if (selected == m_backendDesktopId) {
        return;
    }
    m_backendDesktopId = selected;
    applyShellLaunchers();
    Q_EMIT internetExplorerBackendChanged();
    if (rowCount() > 0) {
        Q_EMIT dataChanged(index(0, 0), index(rowCount() - 1, 0));
    }
}

bool Aero7TasksModel::isInternetExplorerTask(const QModelIndex &index) const
{
    if (!index.isValid() || m_backendDesktopId.isEmpty()) {
        return false;
    }
    const QUrl launcher = TaskManager::TasksModel::data(
        index, TaskManager::AbstractTasksModel::LauncherUrlWithoutIcon).toUrl();
    if (launcher.toString(QUrl::RemoveQuery)
        == launcherForDesktopId(m_backendDesktopId)) {
        return true;
    }
    QString appId = TaskManager::TasksModel::data(
        index, TaskManager::AbstractTasksModel::AppId).toString();
    appId.remove(QStringLiteral(".desktop"), Qt::CaseInsensitive);
    QString backendId = m_backendDesktopId;
    backendId.chop(8);
    return appId.compare(backendId, Qt::CaseInsensitive) == 0;
}

bool Aero7TasksModel::isLauncherTask(const QModelIndex &index) const
{
    return TaskManager::TasksModel::data(
        index, TaskManager::AbstractTasksModel::IsLauncher).toBool();
}

QVariant Aero7TasksModel::data(const QModelIndex &index, int role) const
{
    if (!isInternetExplorerTask(index)) {
        return TaskManager::TasksModel::data(index, role);
    }
    switch (role) {
    case Qt::DisplayRole:
    case TaskManager::AbstractTasksModel::AppName:
        return QStringLiteral("Internet Explorer");
    case Qt::DecorationRole:
        return QIcon::fromTheme(QStringLiteral("aero7-internet-explorer"));
    case TaskManager::AbstractTasksModel::GenericName:
        return QStringLiteral("Web Browser");
    case TaskManager::AbstractTasksModel::AppId:
        return QString::fromLatin1(InternetExplorerDesktopId);
    case TaskManager::AbstractTasksModel::LauncherUrl:
    case TaskManager::AbstractTasksModel::LauncherUrlWithoutIcon:
        return QUrl(launcherForDesktopId(QString::fromLatin1(InternetExplorerDesktopId)));
    default:
        return TaskManager::TasksModel::data(index, role);
    }
}

void Aero7TasksModel::launchInternetExplorer(const QList<QUrl> &urls, bool newWindow)
{
    const KService::Ptr service = KService::serviceByStorageId(
        QString::fromLatin1(InternetExplorerDesktopId));
    if (!service || !service->isValid()) {
        return;
    }
    KIO::ApplicationLauncherJob *job = nullptr;
    if (newWindow) {
        for (const KServiceAction &action : service->actions()) {
            if (action.name() == QLatin1String("NewWindow")) {
                job = new KIO::ApplicationLauncherJob(action, this);
                break;
            }
        }
    }
    if (!job) {
        job = new KIO::ApplicationLauncherJob(service, this);
    }
    job->setUrls(urls);
    job->start();
}

bool Aero7TasksModel::requestAddLauncher(const QUrl &url)
{
    return TaskManager::TasksModel::requestAddLauncher(toBackendLauncher(url));
}

bool Aero7TasksModel::requestRemoveLauncher(const QUrl &url)
{
    return TaskManager::TasksModel::requestRemoveLauncher(toBackendLauncher(url));
}

bool Aero7TasksModel::requestAddLauncherToActivity(const QUrl &url, const QString &activity)
{
    return TaskManager::TasksModel::requestAddLauncherToActivity(toBackendLauncher(url), activity);
}

bool Aero7TasksModel::requestRemoveLauncherFromActivity(const QUrl &url, const QString &activity)
{
    return TaskManager::TasksModel::requestRemoveLauncherFromActivity(toBackendLauncher(url), activity);
}

QStringList Aero7TasksModel::launcherActivities(const QUrl &url)
{
    return TaskManager::TasksModel::launcherActivities(toBackendLauncher(url));
}

int Aero7TasksModel::launcherPosition(const QUrl &url) const
{
    return TaskManager::TasksModel::launcherPosition(toBackendLauncher(url));
}

void Aero7TasksModel::requestActivate(const QModelIndex &index)
{
    if (isInternetExplorerTask(index) && isLauncherTask(index)) {
        launchInternetExplorer();
        return;
    }
    TaskManager::TasksModel::requestActivate(index);
}

void Aero7TasksModel::requestNewInstance(const QModelIndex &index)
{
    if (isInternetExplorerTask(index)) {
        launchInternetExplorer({}, true);
        return;
    }
    TaskManager::TasksModel::requestNewInstance(index);
}

void Aero7TasksModel::requestOpenUrls(const QModelIndex &index, const QList<QUrl> &urls)
{
    if (isInternetExplorerTask(index)) {
        launchInternetExplorer(urls);
        return;
    }
    TaskManager::TasksModel::requestOpenUrls(index, urls);
}
