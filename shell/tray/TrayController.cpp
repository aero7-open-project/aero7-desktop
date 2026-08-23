// SPDX-License-Identifier: MIT
#include "TrayController.h"

#include <NetworkManagerQt/AccessPoint>
#include <NetworkManagerQt/ActiveConnection>
#include <NetworkManagerQt/Connection>
#include <NetworkManagerQt/ConnectionSettings>
#include <NetworkManagerQt/Device>
#include <NetworkManagerQt/Manager>
#include <NetworkManagerQt/Settings>
#include <NetworkManagerQt/WirelessDevice>

#include <Solid/Device>
#include <Solid/StorageAccess>
#include <Solid/StorageDrive>
#include <Solid/StorageVolume>

#include <PulseAudioQt/Client>
#include <PulseAudioQt/Context>
#include <PulseAudioQt/Device>
#include <PulseAudioQt/Server>
#include <PulseAudioQt/Sink>
#include <PulseAudioQt/SinkInput>
#include <PulseAudioQt/Source>

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSet>
#include <QTimer>

#include <algorithm>

namespace
{
int percent(qint64 volume)
{
    return std::clamp(qRound(100.0 * volume / PulseAudioQt::normalVolume()), 0, 150);
}

QString statusText(NetworkManager::Status status)
{
    switch (status) {
    case NetworkManager::Connected: return QStringLiteral("Internet access");
    case NetworkManager::ConnectedSiteOnly: return QStringLiteral("Limited network access");
    case NetworkManager::ConnectedLinkLocal: return QStringLiteral("Local network only");
    case NetworkManager::Connecting: return QStringLiteral("Connecting…");
    case NetworkManager::Disconnecting: return QStringLiteral("Disconnecting…");
    case NetworkManager::Disconnected: return QStringLiteral("Not connected");
    case NetworkManager::Asleep: return QStringLiteral("Networking is off");
    default: return QStringLiteral("Network status unavailable");
    }
}
}

TrayController::TrayController(QObject *parent)
    : QObject(parent)
    , m_audio(PulseAudioQt::Context::instance())
    , m_networkUnavailableForTest(qEnvironmentVariableIsSet("AERO7_TEST_NETWORK_UNAVAILABLE"))
    , m_audioUnavailableForTest(qEnvironmentVariableIsSet("AERO7_TEST_AUDIO_UNAVAILABLE"))
{
    connect(NetworkManager::notifier(), &NetworkManager::Notifier::statusChanged,
            this, &TrayController::refreshNetwork);
    connect(NetworkManager::notifier(), &NetworkManager::Notifier::wirelessEnabledChanged,
            this, &TrayController::refreshNetwork);
    connect(NetworkManager::notifier(), &NetworkManager::Notifier::activeConnectionsChanged,
            this, &TrayController::refreshNetwork);
    connect(NetworkManager::notifier(), &NetworkManager::Notifier::deviceAdded,
            this, &TrayController::refreshNetwork);
    connect(NetworkManager::notifier(), &NetworkManager::Notifier::deviceRemoved,
            this, &TrayController::refreshNetwork);

    auto *fast = new QTimer(this);
    fast->setInterval(1500);
    connect(fast, &QTimer::timeout, this, [this]() {
        refreshAudio();
        refreshStatusItems();
    });
    fast->start();
    auto *slow = new QTimer(this);
    slow->setInterval(5000);
    connect(slow, &QTimer::timeout, this, [this]() {
        refreshNetwork();
        refreshPower();
        refreshRemovableDevices();
    });
    slow->start();
    refreshNetwork();
    refreshAudio();
    refreshPower();
    refreshStatusItems();
    refreshRemovableDevices();
}

QString TrayController::networkStatus() const { return m_networkStatus; }
bool TrayController::networkingEnabled() const { return m_networkingEnabled; }
bool TrayController::wirelessEnabled() const { return m_wirelessEnabled; }
QVariantList TrayController::networks() const { return m_networks; }
bool TrayController::audioAvailable() const { return !m_audioUnavailableForTest && m_audio && m_audio->isValid() && defaultSink(); }
int TrayController::volume() const { return defaultSink() ? percent(defaultSink()->volume()) : 0; }
bool TrayController::muted() const { return defaultSink() ? defaultSink()->isMuted() : true; }
QVariantList TrayController::outputs() const { return m_outputs; }
QVariantList TrayController::inputs() const { return m_inputs; }
QVariantList TrayController::streams() const { return m_streams; }
bool TrayController::batteryPresent() const { return m_batteryPresent; }
double TrayController::batteryPercentage() const { return m_batteryPercentage; }
bool TrayController::batteryCharging() const { return m_batteryCharging; }
QVariantList TrayController::statusItems() const { return m_statusItems; }
QVariantList TrayController::removableDevices() const { return m_removableDevices; }
int TrayController::screenCount() const { return m_screenCount; }

void TrayController::setScreenCount(int count)
{
    if (m_screenCount == count) return;
    m_screenCount = count;
    Q_EMIT screenCountChanged();
}

void TrayController::refreshNetwork()
{
    if (m_networkUnavailableForTest) {
        m_networkStatus = QStringLiteral("Network service unavailable");
        m_networkingEnabled = false;
        m_wirelessEnabled = false;
        m_networks.clear();
        Q_EMIT networkChanged();
        return;
    }
    m_networkStatus = statusText(NetworkManager::status());
    m_networkingEnabled = NetworkManager::isNetworkingEnabled();
    m_wirelessEnabled = NetworkManager::isWirelessEnabled();
    QVariantList rows;
    QSet<QString> seenSsids;
    const auto connections = NetworkManager::listConnections();

    for (const auto &device : NetworkManager::networkInterfaces()) {
        if (!device || !device->managed()) continue;
        if (device->type() == NetworkManager::Device::Wifi) {
            const auto wireless = qSharedPointerDynamicCast<NetworkManager::WirelessDevice>(device);
            if (!wireless) continue;
            auto accessPoints = wireless->accessPoints();
            std::sort(accessPoints.begin(), accessPoints.end(), [&wireless](const QString &left, const QString &right) {
                return wireless->findAccessPoint(left)->signalStrength() > wireless->findAccessPoint(right)->signalStrength();
            });
            for (const auto &path : accessPoints) {
                const auto ap = wireless->findAccessPoint(path);
                if (!ap || ap->ssid().isEmpty() || seenSsids.contains(ap->ssid())) continue;
                seenSsids.insert(ap->ssid());
                QString savedPath;
                for (const auto &connection : connections) {
                    if (connection && connection->settings()
                        && connection->settings()->connectionType() == NetworkManager::ConnectionSettings::Wireless
                        && connection->name() == ap->ssid()) {
                        savedPath = connection->path();
                        break;
                    }
                }
                const bool active = wireless->activeAccessPoint() && wireless->activeAccessPoint()->uni() == ap->uni();
                const bool secured = ap->capabilities().testFlag(NetworkManager::AccessPoint::Privacy)
                    || ap->wpaFlags() != NetworkManager::AccessPoint::WpaFlags()
                    || ap->rsnFlags() != NetworkManager::AccessPoint::WpaFlags();
                rows.append(QVariantMap{{QStringLiteral("kind"), QStringLiteral("wifi")},
                                        {QStringLiteral("name"), ap->ssid()},
                                        {QStringLiteral("signal"), ap->signalStrength()},
                                        {QStringLiteral("secured"), secured},
                                        {QStringLiteral("active"), active},
                                        {QStringLiteral("device"), wireless->uni()},
                                        {QStringLiteral("accessPoint"), ap->uni()},
                                        {QStringLiteral("savedConnection"), savedPath}});
            }
        } else if (device->type() == NetworkManager::Device::Ethernet) {
            rows.append(QVariantMap{{QStringLiteral("kind"), QStringLiteral("ethernet")},
                                    {QStringLiteral("name"), device->interfaceName()},
                                    {QStringLiteral("signal"), 100},
                                    {QStringLiteral("secured"), false},
                                    {QStringLiteral("active"), device->isActive()},
                                    {QStringLiteral("device"), device->uni()},
                                    {QStringLiteral("accessPoint"), QString{}},
                                    {QStringLiteral("savedConnection"), QString{}}});
        }
    }
    if (rows != m_networks) {
        m_networks = rows;
        Q_EMIT networkChanged();
    } else {
        Q_EMIT networkChanged();
    }
}

void TrayController::requestWirelessScan()
{
    if (m_networkUnavailableForTest) {
        Q_EMIT operationError(QStringLiteral("Network service unavailable"),
                              QStringLiteral("NetworkManager is not available. Start the network service and try again."));
        return;
    }
    for (const auto &device : NetworkManager::networkInterfaces()) {
        const auto wireless = qSharedPointerDynamicCast<NetworkManager::WirelessDevice>(device);
        if (wireless) wireless->requestScan();
    }
}

void TrayController::setNetworkingEnabled(bool enabled)
{
    if (m_networkUnavailableForTest) {
        Q_EMIT operationError(QStringLiteral("Network service unavailable"),
                              QStringLiteral("NetworkManager is not available. Start the network service and try again."));
        return;
    }
    NetworkManager::setNetworkingEnabled(enabled);
}

void TrayController::setWirelessEnabled(bool enabled)
{
    if (m_networkUnavailableForTest) {
        Q_EMIT operationError(QStringLiteral("Network service unavailable"),
                              QStringLiteral("NetworkManager is not available. Start the network service and try again."));
        return;
    }
    NetworkManager::setWirelessEnabled(enabled);
}

void TrayController::connectNetwork(const QString &device, const QString &accessPoint,
                                    const QString &ssid, const QString &savedConnection,
                                    const QString &password)
{
    if (m_networkUnavailableForTest) {
        Q_EMIT operationError(QStringLiteral("Could not connect to %1").arg(ssid),
                              QStringLiteral("NetworkManager is not available. Start the network service and try again."));
        return;
    }
    if (!savedConnection.isEmpty()) {
        NetworkManager::activateConnection(savedConnection, device, accessPoint);
        return;
    }
    auto *process = new QProcess(this);
    QStringList arguments{QStringLiteral("device"), QStringLiteral("wifi"), QStringLiteral("connect"), ssid};
    if (!password.isEmpty()) arguments << QStringLiteral("password") << password;
    connect(process, &QProcess::finished, this, [this, process, ssid](int code) {
        if (code != 0) {
            Q_EMIT operationError(QStringLiteral("Could not connect to %1").arg(ssid),
                                  QString::fromLocal8Bit(process->readAllStandardError()).trimmed());
        }
        process->deleteLater();
        refreshNetwork();
    });
    process->start(QStringLiteral("nmcli"), arguments);
}

void TrayController::disconnectNetwork(const QString &devicePath)
{
    if (m_networkUnavailableForTest) {
        Q_EMIT operationError(QStringLiteral("Network service unavailable"),
                              QStringLiteral("NetworkManager is not available. Start the network service and try again."));
        return;
    }
    const auto device = NetworkManager::findNetworkInterface(devicePath);
    if (device) device->disconnectInterface();
}

PulseAudioQt::Sink *TrayController::defaultSink() const
{
    if (m_audioUnavailableForTest || !m_audio || !m_audio->server()) return nullptr;
    return m_audio->server()->defaultSink();
}

void TrayController::refreshAudio()
{
    if (m_audioUnavailableForTest) {
        m_outputs.clear();
        m_inputs.clear();
        m_streams.clear();
        Q_EMIT audioChanged();
        return;
    }
    QVariantList outputRows;
    if (m_audio) {
        for (auto *sink : m_audio->sinks()) {
            if (!sink || sink->isVirtualDevice()) continue;
            outputRows.append(QVariantMap{{QStringLiteral("index"), sink->index()},
                                          {QStringLiteral("name"), sink->description()},
                                          {QStringLiteral("volume"), percent(sink->volume())},
                                          {QStringLiteral("muted"), sink->isMuted()},
                                          {QStringLiteral("default"), sink->isDefault()}});
        }
        if (outputRows.isEmpty() && defaultSink()) {
            auto *sink = defaultSink();
            outputRows.append(QVariantMap{{QStringLiteral("index"), sink->index()},
                                          {QStringLiteral("name"), sink->description()},
                                          {QStringLiteral("volume"), percent(sink->volume())},
                                          {QStringLiteral("muted"), sink->isMuted()},
                                          {QStringLiteral("default"), true}});
        }
    }
    QVariantList streamRows;
    if (m_audio) {
        for (auto *stream : m_audio->sinkInputs()) {
            if (!stream || stream->isVirtualStream() || !stream->hasVolume()) continue;
            const QString name = stream->properties().value(QStringLiteral("application.name")).toString();
            streamRows.append(QVariantMap{{QStringLiteral("index"), stream->index()},
                                          {QStringLiteral("name"), name.isEmpty() ? stream->name() : name},
                                          {QStringLiteral("icon"), stream->iconName()},
                                          {QStringLiteral("volume"), percent(stream->volume())},
                                          {QStringLiteral("muted"), stream->isMuted()}});
        }
    }
    QVariantList inputRows;
    if (m_audio) {
        for (auto *source : m_audio->sources()) {
            if (!source || source->isVirtualDevice()) continue;
            inputRows.append(QVariantMap{{QStringLiteral("index"), source->index()},
                                         {QStringLiteral("name"), source->description()},
                                         {QStringLiteral("volume"), percent(source->volume())},
                                         {QStringLiteral("muted"), source->isMuted()},
                                         {QStringLiteral("default"), source->isDefault()}});
        }
    }
    m_outputs = outputRows;
    m_inputs = inputRows;
    m_streams = streamRows;
    Q_EMIT audioChanged();
}

void TrayController::setVolume(int value)
{
    if (m_audioUnavailableForTest) {
        Q_EMIT operationError(QStringLiteral("Audio service unavailable"),
                              QStringLiteral("PipeWire or PulseAudio is not available. Start the audio service and try again."));
        return;
    }
    if (auto *sink = defaultSink()) sink->setVolume(PulseAudioQt::normalVolume() * std::clamp(value, 0, 150) / 100);
}

void TrayController::toggleMute()
{
    if (m_audioUnavailableForTest) {
        Q_EMIT operationError(QStringLiteral("Audio service unavailable"),
                              QStringLiteral("PipeWire or PulseAudio is not available. Start the audio service and try again."));
        return;
    }
    if (auto *sink = defaultSink()) sink->setMuted(!sink->isMuted());
}

void TrayController::setDefaultOutput(int index)
{
    if (m_audioUnavailableForTest) {
        Q_EMIT operationError(QStringLiteral("Audio service unavailable"),
                              QStringLiteral("PipeWire or PulseAudio is not available. Start the audio service and try again."));
        return;
    }
    if (!m_audio || index < 0 || index >= m_outputs.size()) return;
    const auto pulseIndex = m_outputs.at(index).toMap().value(QStringLiteral("index")).toUInt();
    for (auto *sink : m_audio->sinks()) {
        if (sink && sink->index() == pulseIndex) {
            sink->setDefault(true);
            sink->switchStreams();
            return;
        }
    }
}

void TrayController::setDefaultInput(int index)
{
    if (m_audioUnavailableForTest) {
        Q_EMIT operationError(QStringLiteral("Audio service unavailable"),
                              QStringLiteral("PipeWire or PulseAudio is not available. Start the audio service and try again."));
        return;
    }
    if (!m_audio || index < 0 || index >= m_inputs.size()) return;
    const auto pulseIndex = m_inputs.at(index).toMap().value(QStringLiteral("index")).toUInt();
    for (auto *source : m_audio->sources()) {
        if (source && source->index() == pulseIndex) {
            source->setDefault(true);
            source->switchStreams();
            return;
        }
    }
}

void TrayController::setStreamVolume(int index, int value)
{
    if (m_audioUnavailableForTest) {
        Q_EMIT operationError(QStringLiteral("Audio service unavailable"),
                              QStringLiteral("PipeWire or PulseAudio is not available. Start the audio service and try again."));
        return;
    }
    if (!m_audio || index < 0 || index >= m_streams.size()) return;
    const auto pulseIndex = m_streams.at(index).toMap().value(QStringLiteral("index")).toUInt();
    for (auto *stream : m_audio->sinkInputs()) {
        if (stream && stream->index() == pulseIndex) {
            stream->setVolume(PulseAudioQt::normalVolume() * std::clamp(value, 0, 150) / 100);
            return;
        }
    }
}

void TrayController::setStreamMuted(int index, bool value)
{
    if (m_audioUnavailableForTest) {
        Q_EMIT operationError(QStringLiteral("Audio service unavailable"),
                              QStringLiteral("PipeWire or PulseAudio is not available. Start the audio service and try again."));
        return;
    }
    if (!m_audio || index < 0 || index >= m_streams.size()) return;
    const auto pulseIndex = m_streams.at(index).toMap().value(QStringLiteral("index")).toUInt();
    for (auto *stream : m_audio->sinkInputs()) {
        if (stream && stream->index() == pulseIndex) {
            stream->setMuted(value);
            return;
        }
    }
}

void TrayController::refreshPower()
{
    QDBusInterface upower(QStringLiteral("org.freedesktop.UPower"), QStringLiteral("/org/freedesktop/UPower"),
                          QStringLiteral("org.freedesktop.UPower"), QDBusConnection::systemBus());
    const QDBusReply<QDBusObjectPath> display = upower.call(QStringLiteral("GetDisplayDevice"));
    bool present = false;
    double percentage = 0;
    bool charging = false;
    if (display.isValid()) {
        QDBusInterface device(QStringLiteral("org.freedesktop.UPower"), display.value().path(),
                              QStringLiteral("org.freedesktop.UPower.Device"), QDBusConnection::systemBus());
        present = device.property("IsPresent").toBool();
        percentage = device.property("Percentage").toDouble();
        const auto state = device.property("State").toUInt();
        charging = state == 1 || state == 4;
    }
    if (present != m_batteryPresent || !qFuzzyCompare(percentage + 1, m_batteryPercentage + 1)
        || charging != m_batteryCharging) {
        m_batteryPresent = present;
        m_batteryPercentage = percentage;
        m_batteryCharging = charging;
        Q_EMIT powerChanged();
    }
}

void TrayController::refreshStatusItems()
{
    QDBusInterface watcher(QStringLiteral("org.kde.StatusNotifierWatcher"), QStringLiteral("/StatusNotifierWatcher"),
                           QStringLiteral("org.kde.StatusNotifierWatcher"));
    const auto identifiers = watcher.property("RegisteredStatusNotifierItems").toStringList();
    QVariantList items;
    for (const auto &identifier : identifiers) {
        const int slash = identifier.indexOf(QLatin1Char('/'));
        const QString service = slash > 0 ? identifier.left(slash) : identifier;
        const QString path = slash > 0 ? identifier.mid(slash) : QStringLiteral("/StatusNotifierItem");
        QDBusInterface item(service, path, QStringLiteral("org.kde.StatusNotifierItem"));
        if (!item.isValid()) continue;
        QString icon = item.property("IconName").toString();
        if (icon.isEmpty()) icon = item.property("AttentionIconName").toString();
        items.append(QVariantMap{{QStringLiteral("service"), service}, {QStringLiteral("path"), path},
                                 {QStringLiteral("title"), item.property("Title").toString()},
                                 {QStringLiteral("icon"), icon.isEmpty() ? QStringLiteral("application-x-executable") : icon},
                                 {QStringLiteral("status"), item.property("Status").toString()}});
    }
    if (items != m_statusItems) {
        m_statusItems = items;
        Q_EMIT statusItemsChanged();
    }
}

void TrayController::activateStatusItem(const QString &service, const QString &path, int x, int y)
{
    QDBusInterface item(service, path, QStringLiteral("org.kde.StatusNotifierItem"));
    if (item.isValid()) item.asyncCall(QStringLiteral("Activate"), x, y);
}

void TrayController::refreshRemovableDevices()
{
    QVariantList rows;
    const auto volumes = Solid::Device::listFromType(Solid::DeviceInterface::StorageVolume);
    for (const auto &volumeDevice : volumes) {
        auto *volume = volumeDevice.as<Solid::StorageVolume>();
        auto *access = volumeDevice.as<Solid::StorageAccess>();
        if (!volume || !access) continue;
        Solid::Device parent = volumeDevice.parent();
        while (parent.isValid() && !parent.is<Solid::StorageDrive>()) parent = parent.parent();
        auto *drive = parent.as<Solid::StorageDrive>();
        if (!drive || !drive->isRemovable()) continue;
        rows.append(QVariantMap{{QStringLiteral("udi"), volumeDevice.udi()},
                                {QStringLiteral("name"), volumeDevice.displayName()},
                                {QStringLiteral("icon"), volumeDevice.icon()},
                                {QStringLiteral("mounted"), access->isAccessible()},
                                {QStringLiteral("path"), access->filePath()},
                                {QStringLiteral("size"), QVariant::fromValue<qulonglong>(volume->size())}});
    }
    if (rows != m_removableDevices) {
        m_removableDevices = rows;
        Q_EMIT removableDevicesChanged();
    }
}

void TrayController::unmountRemovable(const QString &udi)
{
    Solid::Device device(udi);
    auto *access = device.as<Solid::StorageAccess>();
    if (!access || !access->isAccessible()) {
        Q_EMIT operationError(QStringLiteral("Safely remove device"),
                              QStringLiteral("The selected device is not mounted."));
        return;
    }
    connect(access, &Solid::StorageAccess::teardownDone, this,
            [this, udi](Solid::ErrorType error, const QVariant &, const QString &) {
        if (error != Solid::NoError) {
            Q_EMIT operationError(QStringLiteral("Could not safely remove device"),
                                  QStringLiteral("Close files in use on the device and try again."));
        }
        refreshRemovableDevices();
    }, Qt::SingleShotConnection);
    access->teardown();
}

void TrayController::openControlPanel(const QString &setting)
{
    QProcess::startDetached(QStringLiteral("aero7-control-panel"), {QStringLiteral("--setting"), setting});
}

void TrayController::toggleNotifications(const QString &screenName)
{
    QDBusInterface notifications(QStringLiteral("org.aero7.Notifications"),
                                 QStringLiteral("/org/aero7/Notifications"),
                                 QStringLiteral("org.freedesktop.Notifications"));
    notifications.asyncCall(QStringLiteral("toggleHistory"), screenName);
}

QString TrayController::dumpState() const
{
    QJsonObject state{{QStringLiteral("schema"), 1},
                      {QStringLiteral("networkStatus"), m_networkStatus},
                      {QStringLiteral("networkingEnabled"), m_networkingEnabled},
                      {QStringLiteral("wirelessEnabled"), m_wirelessEnabled},
                      {QStringLiteral("networks"), QJsonArray::fromVariantList(m_networks)},
                      {QStringLiteral("audioAvailable"), audioAvailable()},
                      {QStringLiteral("networkBackendAvailable"), !m_networkUnavailableForTest},
                      {QStringLiteral("audioBackendAvailable"), !m_audioUnavailableForTest},
                      {QStringLiteral("volume"), volume()},
                      {QStringLiteral("muted"), muted()},
                      {QStringLiteral("outputs"), QJsonArray::fromVariantList(m_outputs)},
                      {QStringLiteral("inputs"), QJsonArray::fromVariantList(m_inputs)},
                      {QStringLiteral("streams"), QJsonArray::fromVariantList(m_streams)},
                      {QStringLiteral("batteryPresent"), m_batteryPresent},
                      {QStringLiteral("batteryPercentage"), m_batteryPercentage},
                      {QStringLiteral("statusItems"), QJsonArray::fromVariantList(m_statusItems)}};
    state.insert(QStringLiteral("removableDevices"), QJsonArray::fromVariantList(m_removableDevices));
    state.insert(QStringLiteral("screens"), m_screenCount);
    return QString::fromUtf8(QJsonDocument(state).toJson(QJsonDocument::Compact));
}
