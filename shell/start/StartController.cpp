// SPDX-License-Identifier: MIT
#include "StartController.h"
#include "ApplicationsModel.h"

#include <QDBusInterface>
#include <QDBusReply>
#include <QDesktopServices>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QStandardPaths>
#include <QSysInfo>
#include <QUrl>

#include <pwd.h>
#include <unistd.h>

StartController::StartController(QObject *parent)
    : QObject(parent)
    , m_applications(new ApplicationsModel(this))
{
    connect(m_applications, &ApplicationsModel::applicationLaunched, this, &StartController::hide);
}

QObject *StartController::applications() const { return m_applications; }
QString StartController::userName() const
{
    if (const auto *entry = getpwuid(geteuid())) {
        const auto displayName = QString::fromLocal8Bit(entry->pw_gecos).section(QLatin1Char(','), 0, 0).trimmed();
        if (!displayName.isEmpty()) return displayName;
        return QString::fromLocal8Bit(entry->pw_name);
    }
    return qEnvironmentVariable("USER");
}
QString StartController::hostName() const { return QSysInfo::machineHostName(); }

QString StartController::userIcon() const
{
    QDBusInterface accounts(QStringLiteral("org.freedesktop.Accounts"), QStringLiteral("/org/freedesktop/Accounts"),
                            QStringLiteral("org.freedesktop.Accounts"), QDBusConnection::systemBus());
    const QDBusReply<QDBusObjectPath> userPath = accounts.call(QStringLiteral("FindUserByName"),
                                                                QString::fromLocal8Bit(qgetenv("USER")));
    if (!userPath.isValid()) return {};
    QDBusInterface user(QStringLiteral("org.freedesktop.Accounts"), userPath.value().path(),
                        QStringLiteral("org.freedesktop.Accounts.User"), QDBusConnection::systemBus());
    const auto icon = user.property("IconFile").toString();
    return QFileInfo::exists(icon) ? QUrl::fromLocalFile(icon).toString() : QString{};
}

namespace
{
bool loginCapability(const QString &method)
{
    QDBusInterface login(QStringLiteral("org.freedesktop.login1"), QStringLiteral("/org/freedesktop/login1"),
                         QStringLiteral("org.freedesktop.login1.Manager"), QDBusConnection::systemBus());
    const QDBusReply<QString> reply = login.call(method);
    return reply.isValid() && (reply.value() == QStringLiteral("yes") || reply.value() == QStringLiteral("challenge"));
}
}

bool StartController::canSuspend() const { return loginCapability(QStringLiteral("CanSuspend")); }
bool StartController::canHibernate() const { return loginCapability(QStringLiteral("CanHibernate")); }
bool StartController::visible() const { return m_visible; }

void StartController::setVisible(bool visible)
{
    if (m_visible == visible) return;
    m_visible = visible;
    Q_EMIT visibleChanged();
}

QString StartController::screenName() const { return m_screenName; }

void StartController::setScreenName(const QString &name)
{
    if (m_screenName == name) return;
    m_screenName = name;
    Q_EMIT screenNameChanged();
}

void StartController::openLocation(const QString &location)
{
    QString path;
    if (location == QStringLiteral("home")) path = QDir::homePath();
    else if (location == QStringLiteral("documents")) path = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    else if (location == QStringLiteral("pictures")) path = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    else if (location == QStringLiteral("music")) path = QStandardPaths::writableLocation(QStandardPaths::MusicLocation);
    else if (location == QStringLiteral("computer")) path = QStringLiteral("/");
    if (!path.isEmpty()) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
        hide();
    }
}

void StartController::openControlPanel()
{
    if (QProcess::startDetached(QStringLiteral("aero7-control-panel"), {})) hide();
    else Q_EMIT operationError(QStringLiteral("Control Panel is unavailable"),
                               QStringLiteral("Install or repair the aero7-desktop package."));
}

void StartController::openControlPanelSetting(const QString &setting)
{
    if (QProcess::startDetached(QStringLiteral("aero7-control-panel"), {QStringLiteral("--setting"), setting})) hide();
    else Q_EMIT operationError(QStringLiteral("Control Panel page is unavailable"),
                               QStringLiteral("The requested settings component could not be started."));
}

void StartController::openDevicesAndPrinters()
{
    if (QProcess::startDetached(QStringLiteral("aero7-control-panel"),
                                {QStringLiteral("--setting"), QStringLiteral("device-actions")})) hide();
    else Q_EMIT operationError(QStringLiteral("Devices and Printers is unavailable"),
                               QStringLiteral("Install or repair the Aero7 Control Panel."));
}

void StartController::lockSession()
{
    QDBusInterface screenSaver(QStringLiteral("org.freedesktop.ScreenSaver"), QStringLiteral("/ScreenSaver"),
                               QStringLiteral("org.freedesktop.ScreenSaver"));
    screenSaver.asyncCall(QStringLiteral("Lock"));
    hide();
}

void StartController::requestPowerAction(const QString &action)
{
    if (action == QStringLiteral("sleep") || action == QStringLiteral("hibernate")) {
        QDBusInterface login(QStringLiteral("org.freedesktop.login1"), QStringLiteral("/org/freedesktop/login1"),
                             QStringLiteral("org.freedesktop.login1.Manager"), QDBusConnection::systemBus());
        login.asyncCall(action == QStringLiteral("sleep") ? QStringLiteral("Suspend") : QStringLiteral("Hibernate"), false);
    } else {
        QDBusInterface prompt(QStringLiteral("org.kde.LogoutPrompt"), QStringLiteral("/LogoutPrompt"),
                              QStringLiteral("org.kde.LogoutPrompt"));
        if (action == QStringLiteral("logout")) prompt.asyncCall(QStringLiteral("promptLogout"));
        else if (action == QStringLiteral("restart")) prompt.asyncCall(QStringLiteral("promptReboot"));
        else if (action == QStringLiteral("shutdown")) prompt.asyncCall(QStringLiteral("promptShutDown"));
    }
    hide();
}

void StartController::toggle() { Q_EMIT toggleRequested({}); }
void StartController::toggleOnScreen(const QString &requestedScreen) { Q_EMIT toggleRequested(requestedScreen); }
void StartController::hide() { Q_EMIT hideRequested(); }
void StartController::setSearchQuery(const QString &query) { m_applications->setQuery(query); }
bool StartController::launchStorageId(const QString &storageId)
{
    if (m_applications->launchStorageId(storageId)) return true;
    Q_EMIT operationError(QStringLiteral("Program unavailable"),
                          QStringLiteral("The selected program is missing, invalid, or could not be started."));
    return false;
}

QString StartController::dumpState() const
{
    QJsonObject state;
    state.insert(QStringLiteral("schema"), 1);
    state.insert(QStringLiteral("visible"), m_visible);
    state.insert(QStringLiteral("screen"), m_screenName);
    state.insert(QStringLiteral("user"), userName());
    state.insert(QStringLiteral("userIcon"), userIcon());
    state.insert(QStringLiteral("canSuspend"), canSuspend());
    state.insert(QStringLiteral("canHibernate"), canHibernate());
    state.insert(QStringLiteral("applications"), QJsonDocument::fromJson(m_applications->dumpState().toUtf8()).object());
    return QString::fromUtf8(QJsonDocument(state).toJson(QJsonDocument::Compact));
}
