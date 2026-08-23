// SPDX-License-Identifier: MIT
#include "NotificationServer.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTextDocument>

#include <algorithm>

NotificationServer::NotificationServer(QObject *parent)
    : QObject(parent)
{
    loadHistory();
}

QVariantMap NotificationServer::current() const { return m_current; }
bool NotificationServer::hasCurrent() const { return !m_current.isEmpty(); }
QVariantList NotificationServer::history() const { return m_history; }
bool NotificationServer::historyVisible() const { return m_historyVisible; }

void NotificationServer::setHistoryVisible(bool visible)
{
    if (m_historyVisible == visible) return;
    m_historyVisible = visible;
    Q_EMIT historyVisibleChanged();
}

QString NotificationServer::historyScreen() const { return m_historyScreen; }
void NotificationServer::setHistoryScreen(const QString &screen)
{
    if (m_historyScreen == screen) return;
    m_historyScreen = screen;
    Q_EMIT historyScreenChanged();
}

bool NotificationServer::ownsFreedesktop() const { return m_ownsFreedesktop; }
void NotificationServer::setOwnsFreedesktop(bool owns)
{
    if (m_ownsFreedesktop == owns) return;
    m_ownsFreedesktop = owns;
    Q_EMIT ownsFreedesktopChanged();
}

void NotificationServer::enableCompatibilityBridge()
{
    m_compatibilityMode = true;
}

uint NotificationServer::Notify(const QString &appName, uint replacesId, const QString &appIcon,
                                const QString &summary, const QString &body,
                                const QStringList &actions, const QVariantMap &hints, int timeout)
{
    const uint id = replacesId != 0 && indexForId(replacesId) >= 0 ? replacesId : m_nextId++;
    QTextDocument bodyDocument;
    bodyDocument.setHtml(body);
    QVariantList actionRows;
    for (int index = 0; index + 1 < actions.size(); index += 2) {
        actionRows.append(QVariantMap{{QStringLiteral("key"), actions.at(index)},
                                      {QStringLiteral("label"), actions.at(index + 1)}});
    }
    const int urgency = hints.value(QStringLiteral("urgency"), 1).toInt();
    QVariantMap entry{{QStringLiteral("id"), id},
                      {QStringLiteral("appName"), appName.isEmpty() ? QStringLiteral("Application") : appName},
                      {QStringLiteral("icon"), appIcon.isEmpty() ? QStringLiteral("dialog-information") : appIcon},
                      {QStringLiteral("summary"), summary},
                      {QStringLiteral("body"), bodyDocument.toPlainText()},
                      {QStringLiteral("actions"), actionRows},
                      {QStringLiteral("urgency"), urgency},
                      {QStringLiteral("timeout"), timeout == 0 ? 0
                          : timeout < 0 ? 5000 : std::clamp(timeout, 1500, 30000)},
                      {QStringLiteral("timestamp"), QDateTime::currentDateTime().toString(Qt::ISODate)}};
    const int existing = indexForId(id);
    if (existing >= 0) m_history[existing] = entry;
    else m_history.prepend(entry);
    while (m_history.size() > 100) m_history.removeLast();
    m_current = entry;
    saveHistory();
    Q_EMIT historyChanged();
    Q_EMIT currentChanged();
    return id;
}

void NotificationServer::CloseNotification(uint id)
{
    if (m_compatibilityMode) Q_EMIT compatibilityCloseRequested(id);
    if (m_current.value(QStringLiteral("id")).toUInt() == id) {
        m_current.clear();
        Q_EMIT currentChanged();
    }
    Q_EMIT NotificationClosed(id, 3);
}

QStringList NotificationServer::GetCapabilities() const
{
    return {QStringLiteral("actions"), QStringLiteral("body"), QStringLiteral("icon-static"),
            QStringLiteral("persistence")};
}

void NotificationServer::GetServerInformation(QString &name, QString &vendor, QString &version,
                                              QString &specVersion) const
{
    name = QStringLiteral("Aero7 Notifications");
    vendor = QStringLiteral("Aero7 Open Project");
    version = QStringLiteral("0.2.0");
    specVersion = QStringLiteral("1.2");
}

void NotificationServer::invokeAction(uint id, const QString &actionKey)
{
    if (m_compatibilityMode) Q_EMIT compatibilityActionRequested(id, actionKey);
    else Q_EMIT ActionInvoked(id, actionKey);
    CloseNotification(id);
}

void NotificationServer::expireCurrent()
{
    if (m_current.isEmpty()) return;
    const uint id = m_current.value(QStringLiteral("id")).toUInt();
    if (m_compatibilityMode) Q_EMIT compatibilityExpireRequested(id);
    m_current.clear();
    Q_EMIT currentChanged();
    Q_EMIT NotificationClosed(id, 1);
}

void NotificationServer::dismissCurrent()
{
    if (m_current.isEmpty()) return;
    const uint id = m_current.value(QStringLiteral("id")).toUInt();
    if (m_compatibilityMode) Q_EMIT compatibilityCloseRequested(id);
    m_current.clear();
    Q_EMIT currentChanged();
    Q_EMIT NotificationClosed(id, 2);
}

void NotificationServer::clearHistory()
{
    if (m_compatibilityMode) Q_EMIT compatibilityClearRequested();
    m_history.clear();
    saveHistory();
    Q_EMIT historyChanged();
}

void NotificationServer::ingestCompatibility(uint id, const QString &appName, const QString &appIcon,
                                              const QString &summary, const QString &body,
                                              const QStringList &actionNames, const QStringList &actionLabels,
                                              int urgency, int timeout, const QString &timestamp)
{
    QVariantList actions;
    for (int action = 0; action < std::min(actionNames.size(), actionLabels.size()); ++action) {
        actions.append(QVariantMap{{QStringLiteral("key"), actionNames.at(action)},
                                   {QStringLiteral("label"), actionLabels.at(action)}});
    }
    QTextDocument bodyDocument;
    bodyDocument.setHtml(body);
    QVariantMap entry{{QStringLiteral("id"), id},
                      {QStringLiteral("appName"), appName.isEmpty() ? QStringLiteral("Application") : appName},
                      {QStringLiteral("icon"), appIcon.isEmpty() ? QStringLiteral("dialog-information") : appIcon},
                      {QStringLiteral("summary"), summary},
                      {QStringLiteral("body"), bodyDocument.toPlainText()},
                      {QStringLiteral("actions"), actions},
                      {QStringLiteral("urgency"), urgency},
                      {QStringLiteral("timeout"), timeout == 0 ? 0
                          : timeout < 0 ? 5000 : std::clamp(timeout, 1500, 30000)},
                      {QStringLiteral("timestamp"), timestamp.isEmpty()
                           ? QDateTime::currentDateTime().toString(Qt::ISODate) : timestamp}};
    const int existing = indexForId(id);
    if (existing >= 0) m_history[existing] = entry;
    else m_history.prepend(entry);
    while (m_history.size() > 100) m_history.removeLast();
    m_current = entry;
    saveHistory();
    Q_EMIT historyChanged();
    Q_EMIT currentChanged();
}

void NotificationServer::compatibilityClosed(uint id, uint reason)
{
    if (m_current.value(QStringLiteral("id")).toUInt() == id) {
        m_current.clear();
        Q_EMIT currentChanged();
    }
    Q_EMIT NotificationClosed(id, reason);
}

void NotificationServer::toggleHistory(const QString &screenName)
{
    setHistoryScreen(screenName);
    setHistoryVisible(!m_historyVisible);
}

QString NotificationServer::dumpState() const
{
    const QVariantMap state{{QStringLiteral("schema"), 1},
                            {QStringLiteral("ownsFreedesktop"), m_ownsFreedesktop},
                            {QStringLiteral("hasCurrent"), hasCurrent()},
                            {QStringLiteral("current"), m_current},
                            {QStringLiteral("history"), m_history},
                            {QStringLiteral("historyVisible"), m_historyVisible},
                            {QStringLiteral("historyScreen"), m_historyScreen}};
    return QString::fromUtf8(QJsonDocument::fromVariant(state).toJson(QJsonDocument::Compact));
}

int NotificationServer::indexForId(uint id) const
{
    for (int index = 0; index < m_history.size(); ++index) {
        if (m_history.at(index).toMap().value(QStringLiteral("id")).toUInt() == id) return index;
    }
    return -1;
}

QString NotificationServer::historyPath() const
{
    return QStandardPaths::writableLocation(QStandardPaths::StateLocation)
        + QStringLiteral("/notifications.json");
}

void NotificationServer::loadHistory()
{
    QFile file(historyPath());
    if (!file.open(QIODevice::ReadOnly)) return;
    const auto document = QJsonDocument::fromJson(file.readAll());
    if (!document.isArray()) return;
    m_history = document.array().toVariantList();
    for (const auto &entry : m_history) {
        m_nextId = std::max(m_nextId, entry.toMap().value(QStringLiteral("id")).toUInt() + 1);
    }
}

void NotificationServer::saveHistory() const
{
    QDir().mkpath(QFileInfo(historyPath()).absolutePath());
    QSaveFile file(historyPath());
    if (!file.open(QIODevice::WriteOnly)) return;
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    file.write(QJsonDocument::fromVariant(m_history).toJson(QJsonDocument::Indented));
    if (!file.commit()) qWarning("Could not persist Aero7 notification history");
}
