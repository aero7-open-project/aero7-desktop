// SPDX-License-Identifier: MIT
#pragma once

#include "PinsStore.h"

#include <QObject>
#include <QRect>
#include <QStringList>
#include <QUrl>
#include <QVariantMap>
#include <QVariantList>

namespace TaskManager
{
class TasksModel;
}

class TaskbarController final : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.aero7.Taskbar")
    Q_PROPERTY(QObject *tasksModel READ tasksModel CONSTANT)
    Q_PROPERTY(int screenCount READ screenCount WRITE setScreenCount NOTIFY screenCountChanged)
    Q_PROPERTY(int taskCount READ taskCount NOTIFY taskCountChanged)
    Q_PROPERTY(int launcherCount READ launcherCount NOTIFY launcherCountChanged)
    Q_PROPERTY(bool showingDesktop READ showingDesktop NOTIFY showingDesktopChanged)

public:
    explicit TaskbarController(QObject *parent = nullptr);
    ~TaskbarController() override;

    [[nodiscard]] QObject *tasksModel() const;
    [[nodiscard]] int screenCount() const;
    [[nodiscard]] int taskCount() const;
    [[nodiscard]] int launcherCount() const;
    [[nodiscard]] bool showingDesktop() const;
    void setScreenCount(int count);

    Q_INVOKABLE bool isPinned(const QUrl &launcherUrl) const;
    Q_INVOKABLE void publishDelegateGeometry(int row, const QRect &geometry, QObject *delegate);
    Q_INVOKABLE void toggleStart(const QString &screenName);
    Q_INVOKABLE void beginWindowPeek(const QStringList &windowIds);
    Q_INVOKABLE void endWindowPeek();
    Q_INVOKABLE QVariantList jumpList(int row) const;

public Q_SLOTS:
    Q_SCRIPTABLE void activate(int row);
    Q_SCRIPTABLE void activateChild(int row, int childRow);
    Q_SCRIPTABLE void launchNewInstance(int row);
    Q_SCRIPTABLE void close(int row);
    Q_SCRIPTABLE void closeChild(int row, int childRow);
    Q_SCRIPTABLE bool togglePin(int row);
    Q_SCRIPTABLE bool pinLauncher(const QString &launcherUrl);
    Q_SCRIPTABLE void move(int row, int newPosition);
    Q_SCRIPTABLE void beginDesktopPeek();
    Q_SCRIPTABLE void endDesktopPeek();
    Q_SCRIPTABLE void toggleShowingDesktop();
    Q_SCRIPTABLE bool launchJumpItem(int row, const QVariantMap &item);
    Q_SCRIPTABLE bool toggleJumpPin(int row, const QString &url);
    Q_SCRIPTABLE QString dumpState() const;

Q_SIGNALS:
    void screenCountChanged();
    void taskCountChanged();
    void launcherCountChanged();
    void showingDesktopChanged();
    void operationError(const QString &title, const QString &message);

private Q_SLOTS:
    void handleShowingDesktopChanged(bool showing);

private:
    [[nodiscard]] QString configPath() const;
    [[nodiscard]] QString defaultsPath() const;
    void persistPins();
    [[nodiscard]] QString jumpListsPath() const;
    [[nodiscard]] QVariantMap jumpListState() const;
    bool saveJumpListState(const QVariantMap &state) const;

    TaskManager::TasksModel *m_tasks;
    PinsStore m_pins;
    int m_screenCount = 0;
    bool m_showingDesktop = false;
    bool m_desktopPeekActive = false;
    bool m_desktopPeekRestore = false;
    QStringList m_peekWindows;
};
