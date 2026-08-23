// SPDX-License-Identifier: MIT
#include "ControlPanelController.h"

#include <KApplicationTrader>
#include <KService>

#include <QCoreApplication>
#include <QDBusInterface>
#include <QDBusReply>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSet>
#include <QStandardPaths>
#include <QtMath>

namespace
{
const QSet<QString> allowedBackendKeys = {
    QStringLiteral("personalization"), QStringLiteral("colors"), QStringLiteral("pointers"),
    QStringLiteral("wallpaper"), QStringLiteral("display"), QStringLiteral("night-light"),
    QStringLiteral("mouse"), QStringLiteral("keyboard"), QStringLiteral("touchpad"),
    QStringLiteral("touchscreen"), QStringLiteral("tablet"), QStringLiteral("game-controller"),
    QStringLiteral("virtual-keyboard"), QStringLiteral("sound"), QStringLiteral("sound-theme"),
    QStringLiteral("network-status"), QStringLiteral("network-connections"), QStringLiteral("proxy"),
    QStringLiteral("power"), QStringLiteral("power-details"), QStringLiteral("mobile-power"),
    QStringLiteral("accounts"), QStringLiteral("date-time"), QStringLiteral("region-language"),
    QStringLiteral("default-apps"), QStringLiteral("file-associations"), QStringLiteral("locations"),
    QStringLiteral("device-actions"), QStringLiteral("file-search"), QStringLiteral("recent-files"),
    QStringLiteral("ease"), QStringLiteral("accessibility-details"), QStringLiteral("autostart"),
    QStringLiteral("session"), QStringLiteral("screen-lock"), QStringLiteral("firewall"),
    QStringLiteral("updates"), QStringLiteral("automount"), QStringLiteral("system-overview"),
};

struct SyntheticSetting {
    const char *key;
    const char *name;
    const char *description;
    const char *icon;
};

constexpr SyntheticSetting syntheticSettings[] = {
    {"programs-features", "Programs and Features", "Uninstall or change installed programs.", "system-software-install"},
    {"volume-mixer", "Volume Mixer", "Adjust sound for individual programs.", "audio-volume-high"},
    {"taskbar-start", "Taskbar and Start Menu", "Manage taskbar pins and Start menu behavior.", "preferences-desktop-launch-feedback"},
    {"folder-options", "Folder Options", "Choose personal folders and file browsing defaults.", "system-file-manager"},
    {"devices", "Devices and Printers", "View devices and removable-media actions.", "preferences-desktop-peripherals"},
    {"gadgets", "Desktop Gadgets", "Add and manage Aero7 desktop gadgets.", "preferences-desktop-widgets"},
};

struct DefaultCategory { const char *name; const char *mime; const char *icon; };
constexpr DefaultCategory defaultCategories[] = {
    {"Web browser", "x-scheme-handler/http", "internet-web-browser"},
    {"Email", "x-scheme-handler/mailto", "internet-mail"},
    {"Music", "audio/mpeg", "audio-x-generic"},
    {"Video", "video/mp4", "video-x-generic"},
    {"Images", "image/jpeg", "image-x-generic"},
    {"Text", "text/plain", "text-plain"},
    {"Archives", "application/zip", "package-x-generic"},
    {"PDF", "application/pdf", "application-pdf"},
};
}

ControlPanelController::ControlPanelController(QObject *parent)
    : QObject(parent)
    , m_backendAvailable(!qEnvironmentVariableIsSet("AERO7_TEST_CONTROL_BACKEND_FAILURE"))
    , m_displayBackendAvailable(!qEnvironmentVariableIsSet("AERO7_TEST_DISPLAY_BACKEND_FAILURE"))
{
    m_displayRevertTimer.setInterval(1000);
    connect(&m_displayRevertTimer, &QTimer::timeout, this, [this]() {
        if (--m_displayRevertSeconds <= 0) {
            revertDisplayChange();
            return;
        }
        Q_EMIT displayRevertSecondsChanged();
    });
    loadSettings();
}

void ControlPanelController::loadSettings()
{
    QProcess backend;
    if (m_backendAvailable) {
        backend.start(QStringLiteral("control"), {QStringLiteral("--list-settings-json")});
        if (!backend.waitForFinished(5000) || backend.exitStatus() != QProcess::NormalExit || backend.exitCode() != 0)
            m_backendAvailable = false;
    }
    const auto document = m_backendAvailable
        ? QJsonDocument::fromJson(backend.readAllStandardOutput()) : QJsonDocument{};
    if (document.isArray()) {
        for (const auto &value : document.array()) {
            auto object = value.toObject();
            const auto key = object.value(QStringLiteral("key")).toString();
            if (!allowedBackendKeys.contains(key))
                continue;
            QVariantMap setting = object.toVariantMap();
            setting.insert(QStringLiteral("category"), categoryFor(key));
            setting.insert(QStringLiteral("backend"), QStringLiteral("control-compatibility"));
            m_allSettings.append(setting);
        }
    }

    for (const auto &entry : syntheticSettings) {
        const auto key = QString::fromLatin1(entry.key);
        m_allSettings.append(QVariantMap{
            {QStringLiteral("key"), key},
            {QStringLiteral("name"), QString::fromLatin1(entry.name)},
            {QStringLiteral("description"), QString::fromLatin1(entry.description)},
            {QStringLiteral("icon"), QString::fromLatin1(entry.icon)},
            {QStringLiteral("category"), categoryFor(key)},
            {QStringLiteral("backend"), QStringLiteral("aero7")},
        });
    }
}

QString ControlPanelController::categoryFor(const QString &key)
{
    if (QStringList{QStringLiteral("firewall"), QStringLiteral("updates"), QStringLiteral("power"),
                    QStringLiteral("power-details"), QStringLiteral("mobile-power"), QStringLiteral("system-overview")}.contains(key))
        return QStringLiteral("System and Security");
    if (key.startsWith(QStringLiteral("network")) || key == QStringLiteral("proxy"))
        return QStringLiteral("Network and Internet");
    if (QStringList{QStringLiteral("sound"), QStringLiteral("sound-theme"), QStringLiteral("volume-mixer"),
                    QStringLiteral("devices"), QStringLiteral("device-actions"), QStringLiteral("automount"),
                    QStringLiteral("mouse"), QStringLiteral("keyboard"), QStringLiteral("touchpad"),
                    QStringLiteral("touchscreen"), QStringLiteral("tablet"), QStringLiteral("game-controller"),
                    QStringLiteral("virtual-keyboard")}.contains(key))
        return QStringLiteral("Hardware and Sound");
    if (QStringList{QStringLiteral("programs-features"), QStringLiteral("default-apps"),
                    QStringLiteral("file-associations"), QStringLiteral("autostart")}.contains(key))
        return QStringLiteral("Programs");
    if (key == QStringLiteral("accounts"))
        return QStringLiteral("User Accounts");
    if (QStringList{QStringLiteral("personalization"), QStringLiteral("colors"), QStringLiteral("pointers"),
                    QStringLiteral("wallpaper"), QStringLiteral("display"), QStringLiteral("night-light"),
                    QStringLiteral("taskbar-start"), QStringLiteral("folder-options"), QStringLiteral("locations"),
                    QStringLiteral("file-search"), QStringLiteral("recent-files"), QStringLiteral("gadgets")}.contains(key))
        return QStringLiteral("Appearance and Personalization");
    if (key == QStringLiteral("date-time") || key == QStringLiteral("region-language"))
        return QStringLiteral("Clock, Language and Region");
    if (key == QStringLiteral("ease") || key == QStringLiteral("accessibility-details"))
        return QStringLiteral("Ease of Access");
    return QStringLiteral("System and Security");
}

QVariantList ControlPanelController::settings() const
{
    if (m_query.isEmpty())
        return m_allSettings;
    QVariantList result;
    for (const auto &value : m_allSettings) {
        const auto setting = value.toMap();
        if (setting.value(QStringLiteral("name")).toString().contains(m_query, Qt::CaseInsensitive)
            || setting.value(QStringLiteral("description")).toString().contains(m_query, Qt::CaseInsensitive)
            || setting.value(QStringLiteral("category")).toString().contains(m_query, Qt::CaseInsensitive))
            result.append(value);
    }
    return result;
}

QString ControlPanelController::query() const { return m_query; }
void ControlPanelController::setQuery(const QString &query)
{
    const auto trimmed = query.trimmed();
    if (m_query == trimmed) return;
    m_query = trimmed;
    Q_EMIT queryChanged();
    Q_EMIT settingsChanged();
}

QString ControlPanelController::viewMode() const { return m_viewMode; }
void ControlPanelController::setViewMode(const QString &viewMode)
{
    if (m_viewMode == viewMode) return;
    m_viewMode = viewMode;
    Q_EMIT viewModeChanged();
}

QString ControlPanelController::currentPage() const { return m_currentPage; }
void ControlPanelController::setCurrentPage(const QString &page)
{
    const auto normalized = page.isEmpty() ? QStringLiteral("hub") : page;
    if (m_currentPage == normalized) return;
    m_currentPage = normalized;
    Q_EMIT currentPageChanged();
    if (m_currentPage == QStringLiteral("display")) refreshDisplays();
}

QVariantList ControlPanelController::displayOutputs() const { return m_displayOutputs; }
QString ControlPanelController::displayStatus() const { return m_displayStatus; }
bool ControlPanelController::displayChangePending() const { return m_displayRevertTimer.isActive(); }
int ControlPanelController::displayRevertSeconds() const { return m_displayRevertSeconds; }
QVariantList ControlPanelController::defaultPrograms() const { return m_defaultPrograms; }
QString ControlPanelController::gadgetStatus() const { return m_gadgetStatus; }

void ControlPanelController::refreshDefaultPrograms()
{
    QVariantList categories;
    for (const auto &category : defaultCategories) {
        const auto mime = QString::fromLatin1(category.mime);
        QProcess query;
        query.start(QStringLiteral("xdg-mime"), {QStringLiteral("query"), QStringLiteral("default"), mime});
        query.waitForFinished(3000);
        const auto current = QString::fromLocal8Bit(query.readAllStandardOutput()).trimmed();
        QVariantList candidates;
        QSet<QString> seen;
        for (const auto &service : KApplicationTrader::queryByMimeType(mime)) {
            if (!service || service->noDisplay() || seen.contains(service->storageId())) continue;
            seen.insert(service->storageId());
            candidates.append(QVariantMap{{QStringLiteral("desktopId"), service->storageId()},
                                          {QStringLiteral("name"), service->name()},
                                          {QStringLiteral("icon"), service->icon()}});
        }
        categories.append(QVariantMap{{QStringLiteral("name"), QString::fromLatin1(category.name)},
                                      {QStringLiteral("mimeType"), mime},
                                      {QStringLiteral("icon"), QString::fromLatin1(category.icon)},
                                      {QStringLiteral("current"), current},
                                      {QStringLiteral("candidates"), candidates}});
    }
    m_defaultPrograms = categories;
    Q_EMIT defaultProgramsChanged();
}

bool ControlPanelController::setDefaultProgram(const QString &mimeType, const QString &desktopId)
{
    if (mimeType.isEmpty() || desktopId.isEmpty()) return false;
    QProcess process;
    process.start(QStringLiteral("xdg-mime"), {QStringLiteral("default"), desktopId, mimeType});
    const bool success = process.waitForFinished(5000) && process.exitStatus() == QProcess::NormalExit
        && process.exitCode() == 0;
    if (!success)
        Q_EMIT operationError(QStringLiteral("Could not change default program"),
                              QString::fromLocal8Bit(process.readAllStandardError()).trimmed());
    refreshDefaultPrograms();
    return success;
}

bool ControlPanelController::setWallpaper(const QString &url)
{
    QDBusInterface desktop(QStringLiteral("org.aero7.Desktop"), QStringLiteral("/Desktop"),
                           QStringLiteral("org.aero7.Desktop"));
    const QDBusReply<bool> reply = desktop.call(QStringLiteral("setWallpaper"), url);
    if (!reply.isValid() || !reply.value()) {
        Q_EMIT operationError(QStringLiteral("Could not set desktop background"),
                              QStringLiteral("Choose a readable local image, and make sure the Aero7 desktop service is running."));
        return false;
    }
    return true;
}

bool ControlPanelController::addDesktopGadget(const QString &type)
{
    QDBusInterface desktop(QStringLiteral("org.aero7.Desktop"), QStringLiteral("/Desktop"),
                           QStringLiteral("org.aero7.Desktop"));
    const QDBusReply<QString> reply = desktop.call(QStringLiteral("addGadget"), type, QString{});
    const bool success = reply.isValid() && !reply.value().isEmpty();
    m_gadgetStatus = success
        ? QStringLiteral("The %1 gadget was added to the primary desktop.").arg(type)
        : QStringLiteral("The gadget could not be added. Make sure the Aero7 desktop service is running.");
    Q_EMIT gadgetStatusChanged();
    if (!success) Q_EMIT operationError(QStringLiteral("Could not add desktop gadget"), m_gadgetStatus);
    return success;
}

void ControlPanelController::setDisplayStatus(const QString &status)
{
    if (m_displayStatus == status) return;
    m_displayStatus = status;
    Q_EMIT displayStatusChanged();
}

bool ControlPanelController::runKScreen(const QStringList &arguments, QByteArray *standardOutput)
{
    if (!m_displayBackendAvailable || QStandardPaths::findExecutable(QStringLiteral("kscreen-doctor")).isEmpty())
        return false;
    QProcess process;
    process.start(QStringLiteral("kscreen-doctor"), arguments);
    if (!process.waitForFinished(10000) || process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        const auto error = QString::fromLocal8Bit(process.readAllStandardError()).trimmed();
        setDisplayStatus(error.isEmpty() ? QStringLiteral("The KScreen display backend did not respond.") : error);
        return false;
    }
    if (standardOutput) *standardOutput = process.readAllStandardOutput();
    return true;
}

void ControlPanelController::refreshDisplays()
{
    QByteArray output;
    if (!runKScreen({QStringLiteral("-j")}, &output)) {
        m_displayOutputs.clear();
        Q_EMIT displayOutputsChanged();
        Q_EMIT operationError(QStringLiteral("Display settings unavailable"),
                              QStringLiteral("The KScreen display backend is not responding. Your current display configuration was left unchanged."));
        return;
    }
    const auto document = QJsonDocument::fromJson(output);
    if (!document.isObject() || !document.object().value(QStringLiteral("outputs")).isArray()) {
        setDisplayStatus(QStringLiteral("KScreen returned an invalid display configuration."));
        return;
    }
    m_displaySnapshot = document.object();
    QVariantList outputs;
    int number = 1;
    for (const auto &value : document.object().value(QStringLiteral("outputs")).toArray()) {
        const auto object = value.toObject();
        if (!object.value(QStringLiteral("connected")).toBool()) continue;
        QVariantList modes;
        for (const auto &modeValue : object.value(QStringLiteral("modes")).toArray()) {
            const auto mode = modeValue.toObject();
            const auto size = mode.value(QStringLiteral("size")).toObject();
            modes.append(QVariantMap{{QStringLiteral("id"), mode.value(QStringLiteral("id")).toString()},
                                     {QStringLiteral("name"), mode.value(QStringLiteral("name")).toString()},
                                     {QStringLiteral("width"), size.value(QStringLiteral("width")).toInt()},
                                     {QStringLiteral("height"), size.value(QStringLiteral("height")).toInt()},
                                     {QStringLiteral("refreshRate"), mode.value(QStringLiteral("refreshRate")).toDouble()}});
        }
        const auto position = object.value(QStringLiteral("pos")).toObject();
        outputs.append(QVariantMap{{QStringLiteral("id"), object.value(QStringLiteral("id")).toInt()},
                                   {QStringLiteral("number"), number++},
                                   {QStringLiteral("name"), object.value(QStringLiteral("name")).toString()},
                                   {QStringLiteral("enabled"), object.value(QStringLiteral("enabled")).toBool()},
                                   {QStringLiteral("primary"), object.value(QStringLiteral("priority")).toInt() == 1},
                                   {QStringLiteral("priority"), object.value(QStringLiteral("priority")).toInt()},
                                   {QStringLiteral("currentModeId"), object.value(QStringLiteral("currentModeId")).toString()},
                                   {QStringLiteral("rotation"), object.value(QStringLiteral("rotation")).toInt()},
                                   {QStringLiteral("scale"), object.value(QStringLiteral("scale")).toDouble(1)},
                                   {QStringLiteral("x"), position.value(QStringLiteral("x")).toInt()},
                                   {QStringLiteral("y"), position.value(QStringLiteral("y")).toInt()},
                                   {QStringLiteral("modes"), modes}});
    }
    m_displayOutputs = outputs;
    setDisplayStatus(QStringLiteral("%1 connected display(s) detected.").arg(outputs.size()));
    Q_EMIT displayOutputsChanged();
}

QStringList ControlPanelController::restoreDisplayArguments() const
{
    QStringList arguments;
    for (const auto &value : m_displaySnapshot.value(QStringLiteral("outputs")).toArray()) {
        const auto output = value.toObject();
        if (!output.value(QStringLiteral("connected")).toBool()) continue;
        const auto id = QString::number(output.value(QStringLiteral("id")).toInt());
        const auto prefix = QStringLiteral("output.%1.").arg(id);
        arguments << prefix + (output.value(QStringLiteral("enabled")).toBool() ? QStringLiteral("enable") : QStringLiteral("disable"));
        if (!output.value(QStringLiteral("enabled")).toBool()) continue;
        const auto position = output.value(QStringLiteral("pos")).toObject();
        const auto rotation = output.value(QStringLiteral("rotation")).toInt();
        const auto rotationName = rotation == 2 ? QStringLiteral("left")
            : rotation == 4 ? QStringLiteral("inverted")
            : rotation == 8 ? QStringLiteral("right") : QStringLiteral("normal");
        arguments << prefix + QStringLiteral("mode.%1").arg(output.value(QStringLiteral("currentModeId")).toString())
                  << prefix + QStringLiteral("position.%1,%2").arg(position.value(QStringLiteral("x")).toInt()).arg(position.value(QStringLiteral("y")).toInt())
                  << prefix + QStringLiteral("rotation.%1").arg(rotationName)
                  << prefix + QStringLiteral("scale.%1").arg(output.value(QStringLiteral("scale")).toDouble(1), 0, 'f', 2)
                  << prefix + QStringLiteral("priority.%1").arg(output.value(QStringLiteral("priority")).toInt());
    }
    return arguments;
}

bool ControlPanelController::applyDisplay(int outputId, const QString &modeId, const QString &rotation,
                                          double scale, bool enabled, bool primary, int x, int y)
{
    if (m_displaySnapshot.isEmpty()) refreshDisplays();
    if (m_displaySnapshot.isEmpty()) return false;
    const auto prefix = QStringLiteral("output.%1.").arg(outputId);
    QStringList arguments{prefix + (enabled ? QStringLiteral("enable") : QStringLiteral("disable"))};
    if (enabled) {
        arguments << prefix + QStringLiteral("mode.%1").arg(modeId)
                  << prefix + QStringLiteral("rotation.%1").arg(rotation)
                  << prefix + QStringLiteral("scale.%1").arg(qBound(0.5, scale, 3.0), 0, 'f', 2)
                  << prefix + QStringLiteral("position.%1,%2").arg(x).arg(y);
        if (primary) arguments << prefix + QStringLiteral("priority.1");
    }
    if (!runKScreen(arguments)) {
        Q_EMIT operationError(QStringLiteral("Could not apply display settings"), m_displayStatus);
        return false;
    }
    m_displayRevertSeconds = 15;
    m_displayRevertTimer.start();
    setDisplayStatus(QStringLiteral("Display settings applied. Confirm them within 15 seconds."));
    Q_EMIT displayChangePendingChanged();
    Q_EMIT displayRevertSecondsChanged();
    return true;
}

void ControlPanelController::confirmDisplayChange()
{
    if (!m_displayRevertTimer.isActive()) return;
    m_displayRevertTimer.stop();
    m_displaySnapshot = {};
    setDisplayStatus(QStringLiteral("Display settings kept."));
    Q_EMIT displayChangePendingChanged();
    refreshDisplays();
}

void ControlPanelController::revertDisplayChange()
{
    if (m_displaySnapshot.isEmpty()) return;
    m_displayRevertTimer.stop();
    const auto arguments = restoreDisplayArguments();
    if (!runKScreen(arguments))
        Q_EMIT operationError(QStringLiteral("Could not restore display settings"), m_displayStatus);
    else
        setDisplayStatus(QStringLiteral("Previous display settings restored."));
    m_displaySnapshot = {};
    m_displayRevertSeconds = 0;
    Q_EMIT displayChangePendingChanged();
    Q_EMIT displayRevertSecondsChanged();
    refreshDisplays();
}

bool ControlPanelController::launchDetached(const QString &program, const QStringList &arguments)
{
    return !QStandardPaths::findExecutable(program).isEmpty() && QProcess::startDetached(program, arguments);
}

bool ControlPanelController::launchSetting(const QString &key)
{
    if (key == QStringLiteral("display")) {
        if (!m_displayBackendAvailable) {
            Q_EMIT operationError(QStringLiteral("Display settings unavailable"),
                                  QStringLiteral("The KScreen display backend is not responding. Your current display configuration was left unchanged."));
            return false;
        }
        setCurrentPage(QStringLiteral("display"));
        return true;
    }
    if (key == QStringLiteral("default-apps") || key == QStringLiteral("file-associations")) {
        setCurrentPage(QStringLiteral("defaults"));
        refreshDefaultPrograms();
        return true;
    }
    if (key == QStringLiteral("personalization") || key == QStringLiteral("wallpaper")) {
        setCurrentPage(QStringLiteral("personalization"));
        return true;
    }
    if (key == QStringLiteral("gadgets")) {
        setCurrentPage(QStringLiteral("gadgets"));
        return true;
    }
    QString backendKey = key;
    if (key == QStringLiteral("folder-options")) backendKey = QStringLiteral("locations");
    else if (key == QStringLiteral("devices")) backendKey = QStringLiteral("device-actions");
    else if (key == QStringLiteral("taskbar-start")) backendKey = QStringLiteral("recent-files");

    bool started = false;
    if (allowedBackendKeys.contains(backendKey) && m_backendAvailable)
        started = launchDetached(QStringLiteral("control"), {QStringLiteral("--setting"), backendKey});
    else if (key == QStringLiteral("volume-mixer")) {
        QDBusInterface tray(QStringLiteral("org.aero7.Tray"), QStringLiteral("/Tray"), QStringLiteral("org.aero7.Tray"));
        started = tray.isValid();
        if (started) tray.asyncCall(QStringLiteral("showMixer"));
    } else if (key == QStringLiteral("programs-features")) {
        started = launchDetached(QStringLiteral("aero7-programs-center"));
        if (!started) started = launchDetached(QStringLiteral("control"), {QStringLiteral("--setting"), QStringLiteral("updates")});
    }

    if (!started)
        Q_EMIT operationError(QStringLiteral("Setting unavailable"),
                              QStringLiteral("The supporting component for this setting is not installed or did not respond."));
    return started;
}

QByteArray ControlPanelController::settingsJson() const
{
    return QJsonDocument::fromVariant(settings()).toJson(QJsonDocument::Compact);
}

QString ControlPanelController::dumpState() const
{
    QJsonArray keys;
    QSet<QString> backends;
    for (const auto &value : m_allSettings) {
        const auto item = value.toMap();
        keys.append(item.value(QStringLiteral("key")).toString());
        backends.insert(item.value(QStringLiteral("backend")).toString());
    }
    QJsonObject state{
        {QStringLiteral("schema"), 1},
        {QStringLiteral("settings"), m_allSettings.size()},
        {QStringLiteral("visibleSettings"), settings().size()},
        {QStringLiteral("query"), m_query},
        {QStringLiteral("viewMode"), m_viewMode},
        {QStringLiteral("currentPage"), m_currentPage},
        {QStringLiteral("keys"), keys},
        {QStringLiteral("backends"), QJsonArray::fromStringList(backends.values())},
        {QStringLiteral("backendAvailable"), m_backendAvailable},
        {QStringLiteral("displayBackendAvailable"), m_displayBackendAvailable},
        {QStringLiteral("displayOutputs"), QJsonArray::fromVariantList(m_displayOutputs)},
        {QStringLiteral("displayStatus"), m_displayStatus},
        {QStringLiteral("displayChangePending"), displayChangePending()},
        {QStringLiteral("displayRevertSeconds"), m_displayRevertSeconds},
        {QStringLiteral("defaultPrograms"), QJsonArray::fromVariantList(m_defaultPrograms)},
        {QStringLiteral("gadgetStatus"), m_gadgetStatus},
    };
    return QString::fromUtf8(QJsonDocument(state).toJson(QJsonDocument::Compact));
}
