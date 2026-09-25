/*
    SPDX-FileCopyrightText: 2026 Aero7 Open Project
    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#pragma once

#include <QFileSystemWatcher>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <taskmanager/abstracttasksmodel.h>
#include <taskmanager/tasksmodel.h>

class Aero7TasksModel final : public TaskManager::TasksModel
{
    Q_OBJECT
    Q_PROPERTY(QStringList shellLauncherList READ shellLauncherList WRITE setShellLauncherList NOTIFY shellLauncherListChanged)
    Q_PROPERTY(QString internetExplorerBackend READ internetExplorerBackend NOTIFY internetExplorerBackendChanged)

public:
    explicit Aero7TasksModel(QObject *parent = nullptr);

    QStringList shellLauncherList() const;
    void setShellLauncherList(const QStringList &launchers);
    Q_INVOKABLE void restoreOrAdoptLaunchers(const QStringList &panelLaunchers);
    QString internetExplorerBackend() const;

    QVariant data(const QModelIndex &index, int role) const override;

    Q_INVOKABLE bool requestAddLauncher(const QUrl &url);
    Q_INVOKABLE bool requestRemoveLauncher(const QUrl &url);
    Q_INVOKABLE bool requestAddLauncherToActivity(const QUrl &url, const QString &activity);
    Q_INVOKABLE bool requestRemoveLauncherFromActivity(const QUrl &url, const QString &activity);
    Q_INVOKABLE QStringList launcherActivities(const QUrl &url);
    Q_INVOKABLE int launcherPosition(const QUrl &url) const;
    Q_INVOKABLE bool move(int row, int newPos, const QModelIndex &parent = QModelIndex());

    Q_INVOKABLE void requestActivate(const QModelIndex &index) override;
    Q_INVOKABLE void requestNewInstance(const QModelIndex &index) override;
    Q_INVOKABLE void requestOpenUrls(const QModelIndex &index, const QList<QUrl> &urls) override;

Q_SIGNALS:
    void shellLauncherListChanged();
    void internetExplorerBackendChanged();

private:
    static QString taskbarStatePath();
    void saveShellLaunchers();
    void saveTaskOrder();
    void restoreTaskOrder();
    QString taskKeyAt(int row) const;
    static QString configPath();
    static QString policyPath();
    static bool isSafeDesktopId(const QString &desktopId);

    QString effectiveBackend() const;
    QString toBackendLauncher(const QString &launcher) const;
    QString toShellLauncher(const QString &launcher) const;
    QUrl toBackendLauncher(const QUrl &launcher) const;
    QUrl toShellLauncher(const QUrl &launcher) const;
    bool isInternetExplorerTask(const QModelIndex &index) const;
    bool isLauncherTask(const QModelIndex &index) const;
    void launchInternetExplorer(const QList<QUrl> &urls = {}, bool newWindow = false);
    void refreshBackend();
    void refreshWatchPaths();
    void applyShellLaunchers();

    QFileSystemWatcher m_watcher;
    QStringList m_shellLaunchers;
    QStringList m_taskOrder;
    bool m_hasSavedLaunchers = false;
    bool m_restoringTaskOrder = false;
    QTimer m_orderRestoreTimer;
    QString m_backendDesktopId;
    bool m_applyingLaunchers = false;
};
