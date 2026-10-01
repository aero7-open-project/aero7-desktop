/*
    SPDX-FileCopyrightText: 2026 Aero7 Open Project
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <KConfigGroup>
#include <KSharedConfig>

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QStandardPaths>
#include <QStringList>

#include <cerrno>
#include <csignal>
#include <optional>
#include <sys/types.h>
#include <unistd.h>

namespace
{
constexpr auto orcaPath = "/usr/bin/orca";

QString pidFilePath()
{
    QString runtime = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (runtime.isEmpty()) {
        runtime = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    }
    return runtime + QStringLiteral("/aero7-sddm-orca.pid");
}

std::optional<qint64> trackedOrcaPid()
{
    QFile file(pidFilePath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return std::nullopt;
    }

    bool ok = false;
    const qint64 pid = QString::fromLatin1(file.readAll()).trimmed().toLongLong(&ok);
    if (!ok || pid <= 1) {
        return std::nullopt;
    }

    QFile cmdline(QStringLiteral("/proc/%1/cmdline").arg(pid));
    if (!cmdline.open(QIODevice::ReadOnly)) {
        return std::nullopt;
    }
    const QByteArray command = cmdline.readAll();
    if (!command.contains("orca")) {
        return std::nullopt;
    }
    if (::kill(static_cast<pid_t>(pid), 0) != 0 && errno != EPERM) {
        return std::nullopt;
    }
    return pid;
}

bool setNarrator(bool enabled, QString *error)
{
    if (enabled) {
        if (trackedOrcaPid()) {
            return true;
        }
        if (!QFileInfo(QString::fromLatin1(orcaPath)).isExecutable()) {
            *error = QStringLiteral("Orca is not installed");
            return false;
        }

        qint64 pid = 0;
        if (!QProcess::startDetached(QString::fromLatin1(orcaPath), {QStringLiteral("--replace")}, QString(), &pid) || pid <= 1) {
            *error = QStringLiteral("Orca could not be started");
            return false;
        }

        QFile file(pidFilePath());
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            ::kill(static_cast<pid_t>(pid), SIGTERM);
            *error = QStringLiteral("The narrator process could not be tracked safely");
            return false;
        }
        file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
        file.write(QByteArray::number(pid));
        file.write("\n");
        return true;
    }

    const auto pid = trackedOrcaPid();
    if (pid && ::kill(static_cast<pid_t>(*pid), SIGTERM) != 0 && errno != ESRCH) {
        *error = QStringLiteral("The tracked narrator process could not be stopped");
        return false;
    }
    QFile::remove(pidFilePath());
    // The service is socket-activated by Orca. Stop it while the greeter user
    // session is still healthy; the packaged unit drop-in makes this a clean
    // SIGINT shutdown instead of Speech Dispatcher 0.12.1's SIGTERM crash.
    QProcess::execute(QStringLiteral("/usr/bin/systemctl"),
                      {QStringLiteral("--user"), QStringLiteral("stop"), QStringLiteral("speech-dispatcher.service")});
    return true;
}

bool reconfigureKWin(QString *error)
{
    const QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected() || !bus.interface()
        || !bus.interface()->isServiceRegistered(QStringLiteral("org.kde.KWin"))) {
        // Unit tests and X11 sessions do not necessarily have a KWin service.
        // KConfig::Notify remains the correct fallback in those environments.
        return true;
    }

    const QDBusMessage request = QDBusMessage::createMethodCall(
        QStringLiteral("org.kde.KWin"), QStringLiteral("/KWin"),
        QStringLiteral("org.kde.KWin"), QStringLiteral("reconfigure"));
    const QDBusMessage reply = bus.call(request, QDBus::Block, 2000);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        *error = QStringLiteral("The compositor could not reload the keyboard accessibility settings");
        return false;
    }
    return true;
}

QJsonObject stateObject(bool success, const QString &error = {})
{
    const auto config = KSharedConfig::openConfig(QStringLiteral("kaccessrc"));
    const KConfigGroup keyboard(config, QStringLiteral("Keyboard"));
    const KConfigGroup screenReader(config, QStringLiteral("ScreenReader"));

    QJsonObject object{
        {QStringLiteral("success"), success},
        {QStringLiteral("narratorAvailable"), QFileInfo(QString::fromLatin1(orcaPath)).isExecutable()},
        {QStringLiteral("narrator"), trackedOrcaPid().has_value()},
        {QStringLiteral("stickyKeys"), keyboard.readEntry(QStringLiteral("StickyKeys"), false)},
        {QStringLiteral("filterKeys"), keyboard.readEntry(QStringLiteral("SlowKeys"), false)
                && keyboard.readEntry(QStringLiteral("BounceKeys"), false)},
        {QStringLiteral("screenReaderConfigured"), screenReader.readEntry(QStringLiteral("Enabled"), false)},
    };
    if (!error.isEmpty()) {
        object.insert(QStringLiteral("error"), error);
    }
    return object;
}

bool parseBoolean(const QString &value, bool *result)
{
    if (value == QLatin1String("1")) {
        *result = true;
        return true;
    }
    if (value == QLatin1String("0")) {
        *result = false;
        return true;
    }
    return false;
}

int printJson(const QJsonObject &object, int exitCode)
{
    const QByteArray data = QJsonDocument(object).toJson(QJsonDocument::Compact) + '\n';
    QFile output;
    if (!output.open(stdout, QIODevice::WriteOnly)) {
        return 6;
    }
    output.write(data);
    output.close();
    return exitCode;
}
}

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    application.setApplicationName(QStringLiteral("aero7-sddm-accessibility"));

    const QStringList arguments = application.arguments();
    if (arguments.size() == 2 && arguments.at(1) == QLatin1String("status")) {
        return printJson(stateObject(true), 0);
    }
    if (arguments.size() != 5 || arguments.at(1) != QLatin1String("apply")) {
        return printJson({
            {QStringLiteral("success"), false},
            {QStringLiteral("error"), QStringLiteral("Usage: aero7-sddm-accessibility status | apply NARRATOR STICKY FILTER")},
        }, 2);
    }

    bool narrator = false;
    bool stickyKeys = false;
    bool filterKeys = false;
    if (!parseBoolean(arguments.at(2), &narrator)
        || !parseBoolean(arguments.at(3), &stickyKeys)
        || !parseBoolean(arguments.at(4), &filterKeys)) {
        return printJson({
            {QStringLiteral("success"), false},
            {QStringLiteral("error"), QStringLiteral("Accessibility values must be 0 or 1")},
        }, 2);
    }

    if (narrator && !QFileInfo(QString::fromLatin1(orcaPath)).isExecutable()) {
        return printJson(stateObject(false, QStringLiteral("Narrator is unavailable because Orca is not installed")), 3);
    }

    const auto config = KSharedConfig::openConfig(QStringLiteral("kaccessrc"));
    KConfigGroup keyboard(config, QStringLiteral("Keyboard"));
    keyboard.writeEntry(QStringLiteral("StickyKeys"), stickyKeys, KConfig::Notify);
    keyboard.writeEntry(QStringLiteral("StickyKeysLatch"), true, KConfig::Notify);
    keyboard.writeEntry(QStringLiteral("SlowKeys"), filterKeys, KConfig::Notify);
    keyboard.writeEntry(QStringLiteral("SlowKeysDelay"), 500, KConfig::Notify);
    keyboard.writeEntry(QStringLiteral("BounceKeys"), filterKeys, KConfig::Notify);
    keyboard.writeEntry(QStringLiteral("BounceKeysDelay"), 500, KConfig::Notify);
    KConfigGroup screenReader(config, QStringLiteral("ScreenReader"));
    screenReader.writeEntry(QStringLiteral("Enabled"), narrator, KConfig::Notify);
    if (!config->sync()) {
        return printJson(stateObject(false, QStringLiteral("Accessibility settings could not be saved")), 4);
    }

    QString error;
    if (!reconfigureKWin(&error)) {
        return printJson(stateObject(false, error), 5);
    }
    if (!setNarrator(narrator, &error)) {
        screenReader.writeEntry(QStringLiteral("Enabled"), false, KConfig::Notify);
        config->sync();
        return printJson(stateObject(false, error), 6);
    }
    return printJson(stateObject(true), 0);
}
