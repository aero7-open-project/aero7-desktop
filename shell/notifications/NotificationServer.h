// SPDX-License-Identifier: MIT
#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

class NotificationServer final : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.Notifications")
    Q_PROPERTY(QVariantMap current READ current NOTIFY currentChanged)
    Q_PROPERTY(bool hasCurrent READ hasCurrent NOTIFY currentChanged)
    Q_PROPERTY(QVariantList history READ history NOTIFY historyChanged)
    Q_PROPERTY(bool historyVisible READ historyVisible WRITE setHistoryVisible NOTIFY historyVisibleChanged)
    Q_PROPERTY(QString historyScreen READ historyScreen WRITE setHistoryScreen NOTIFY historyScreenChanged)
    Q_PROPERTY(bool ownsFreedesktop READ ownsFreedesktop WRITE setOwnsFreedesktop NOTIFY ownsFreedesktopChanged)

public:
    explicit NotificationServer(QObject *parent = nullptr);
    QVariantMap current() const;
    bool hasCurrent() const;
    QVariantList history() const;
    bool historyVisible() const;
    void setHistoryVisible(bool visible);
    QString historyScreen() const;
    void setHistoryScreen(const QString &screen);
    bool ownsFreedesktop() const;
    void setOwnsFreedesktop(bool owns);
    void enableCompatibilityBridge();

public Q_SLOTS:
    Q_SCRIPTABLE uint Notify(const QString &appName, uint replacesId, const QString &appIcon,
                             const QString &summary, const QString &body,
                             const QStringList &actions, const QVariantMap &hints, int timeout);
    Q_SCRIPTABLE void CloseNotification(uint id);
    Q_SCRIPTABLE QStringList GetCapabilities() const;
    Q_SCRIPTABLE void GetServerInformation(QString &name, QString &vendor, QString &version,
                                           QString &specVersion) const;
    Q_SCRIPTABLE void invokeAction(uint id, const QString &actionKey);
    Q_SCRIPTABLE void expireCurrent();
    Q_SCRIPTABLE void dismissCurrent();
    Q_SCRIPTABLE void clearHistory();
    Q_SCRIPTABLE void toggleHistory(const QString &screenName);
    Q_SCRIPTABLE void ingestCompatibility(uint id, const QString &appName, const QString &appIcon,
                                          const QString &summary, const QString &body,
                                          const QStringList &actionNames, const QStringList &actionLabels,
                                          int urgency, int timeout, const QString &timestamp);
    Q_SCRIPTABLE void compatibilityClosed(uint id, uint reason);
    Q_SCRIPTABLE QString dumpState() const;

Q_SIGNALS:
    void NotificationClosed(uint id, uint reason);
    void ActionInvoked(uint id, const QString &actionKey);
    void currentChanged();
    void historyChanged();
    void historyVisibleChanged();
    void historyScreenChanged();
    void ownsFreedesktopChanged();
    void compatibilityActionRequested(uint id, const QString &actionKey);
    void compatibilityCloseRequested(uint id);
    void compatibilityExpireRequested(uint id);
    void compatibilityClearRequested();

private:
    void loadHistory();
    void saveHistory() const;
    int indexForId(uint id) const;
    QString historyPath() const;

    QVariantMap m_current;
    QVariantList m_history;
    uint m_nextId = 1;
    bool m_historyVisible = false;
    QString m_historyScreen;
    bool m_ownsFreedesktop = false;
    bool m_compatibilityMode = false;
};
