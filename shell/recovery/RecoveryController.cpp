// SPDX-License-Identifier: MIT
#include "RecoveryController.h"

#include <QDateTime>
#include <QDBusInterface>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QStandardPaths>

namespace
{
const QStringList shellUnits{
    QStringLiteral("plasma-plasmashell.service"), QStringLiteral("aero7-session-setup.service"),
    QStringLiteral("aero7-shell.service")};
}

RecoveryController::RecoveryController(QString component, QObject *parent)
    : QObject(parent), m_component(std::move(component))
{
}

QString RecoveryController::component() const { return m_component; }
QString RecoveryController::message() const
{
    return QStringLiteral("The AeroShell desktop couldn't be started after several attempts.");
}
QString RecoveryController::unit() const
{
    return QStringLiteral("plasma-plasmashell.service");
}

void RecoveryController::restartComponent()
{
    const auto target = unit();
    QProcess::execute(QStringLiteral("systemctl"), {QStringLiteral("--user"), QStringLiteral("reset-failed"), target});
    const bool ok = QProcess::execute(QStringLiteral("systemctl"),
                                      {QStringLiteral("--user"), QStringLiteral("restart"), target}) == 0;
    if (ok) {
        const auto stateRoot = QStandardPaths::writableLocation(QStandardPaths::StateLocation)
            + QStringLiteral("/aero7-desktop");
        QFile::remove(stateRoot + QStringLiteral("/recovery-") + m_component + QStringLiteral(".shown"));
        QFile recovery(stateRoot + QStringLiteral("/recovery.json"));
        if (recovery.open(QIODevice::ReadOnly)) {
            const auto payload = QJsonDocument::fromJson(recovery.readAll()).object();
            recovery.close();
            if (payload.value(QStringLiteral("failed_component")).toString() == m_component)
                QFile::remove(recovery.fileName());
        }
    }
    if (ok) QProcess::execute(QStringLiteral("systemctl"),
                              {QStringLiteral("--user"), QStringLiteral("restart"),
                               QStringLiteral("aero7-session-setup.service"),
                               QStringLiteral("aero7-shell.service")});
    Q_EMIT operationFinished(ok ? QStringLiteral("AeroShell was restarted.")
                                : QStringLiteral("AeroShell still could not start."), ok);
}

void RecoveryController::restartShell()
{
    QStringList arguments{QStringLiteral("--user"), QStringLiteral("reset-failed")};
    arguments.append(shellUnits);
    QProcess::execute(QStringLiteral("systemctl"), arguments);
    arguments = {QStringLiteral("--user"), QStringLiteral("restart")};
    arguments.append(shellUnits);
    const bool ok = QProcess::execute(QStringLiteral("systemctl"), arguments) == 0;
    Q_EMIT operationFinished(ok ? QStringLiteral("The Aero7 shell was restarted.")
                                : QStringLiteral("Some shell components could not restart."), ok);
}

void RecoveryController::resetShellState()
{
    const QString config = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
        + QStringLiteral("/aero7-desktop");
    const QString state = QStandardPaths::writableLocation(QStandardPaths::StateLocation);
    const QString backup = state + QStringLiteral("/reset-backups/aero7-desktop-")
        + QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd-HHmmss"));
    QDir().mkpath(QFileInfo(backup).absolutePath());
    bool ok = true;
    if (QFileInfo::exists(config)) ok = QDir().rename(config, backup);
    if (ok) restartShell();
    else Q_EMIT operationFinished(QStringLiteral("The shell state could not be backed up. Check disk permissions."), false);
}

void RecoveryController::openLogs()
{
    const auto target = QStringLiteral("aero7-shell.service");
    if (!QProcess::startDetached(QStringLiteral("qterminal"),
                                 {QStringLiteral("-e"), QStringLiteral("journalctl"),
                                  QStringLiteral("--user"), QStringLiteral("-n"), QStringLiteral("200"),
                                  QStringLiteral("-u"), target, QStringLiteral("--no-pager")})) {
        Q_EMIT operationFinished(QStringLiteral("Could not open the log viewer."), false);
    }
}

void RecoveryController::signOut()
{
    QDBusInterface prompt(QStringLiteral("org.kde.LogoutPrompt"), QStringLiteral("/LogoutPrompt"),
                          QStringLiteral("org.kde.LogoutPrompt"));
    prompt.asyncCall(QStringLiteral("promptLogout"));
}
