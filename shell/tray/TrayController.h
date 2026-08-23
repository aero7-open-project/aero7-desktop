// SPDX-License-Identifier: MIT
#pragma once

#include <QObject>
#include <QVariantList>

namespace PulseAudioQt { class Context; class Sink; class SinkInput; class Source; }

class TrayController final : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.aero7.Tray")
    Q_PROPERTY(QString networkStatus READ networkStatus NOTIFY networkChanged)
    Q_PROPERTY(bool networkingEnabled READ networkingEnabled NOTIFY networkChanged)
    Q_PROPERTY(bool wirelessEnabled READ wirelessEnabled NOTIFY networkChanged)
    Q_PROPERTY(QVariantList networks READ networks NOTIFY networkChanged)
    Q_PROPERTY(bool audioAvailable READ audioAvailable NOTIFY audioChanged)
    Q_PROPERTY(int volume READ volume NOTIFY audioChanged)
    Q_PROPERTY(bool muted READ muted NOTIFY audioChanged)
    Q_PROPERTY(QVariantList outputs READ outputs NOTIFY audioChanged)
    Q_PROPERTY(QVariantList inputs READ inputs NOTIFY audioChanged)
    Q_PROPERTY(QVariantList streams READ streams NOTIFY audioChanged)
    Q_PROPERTY(bool batteryPresent READ batteryPresent NOTIFY powerChanged)
    Q_PROPERTY(double batteryPercentage READ batteryPercentage NOTIFY powerChanged)
    Q_PROPERTY(bool batteryCharging READ batteryCharging NOTIFY powerChanged)
    Q_PROPERTY(QVariantList statusItems READ statusItems NOTIFY statusItemsChanged)
    Q_PROPERTY(QVariantList removableDevices READ removableDevices NOTIFY removableDevicesChanged)
    Q_PROPERTY(int screenCount READ screenCount WRITE setScreenCount NOTIFY screenCountChanged)

public:
    explicit TrayController(QObject *parent = nullptr);
    QString networkStatus() const;
    bool networkingEnabled() const;
    bool wirelessEnabled() const;
    QVariantList networks() const;
    bool audioAvailable() const;
    int volume() const;
    bool muted() const;
    QVariantList outputs() const;
    QVariantList inputs() const;
    QVariantList streams() const;
    bool batteryPresent() const;
    double batteryPercentage() const;
    bool batteryCharging() const;
    QVariantList statusItems() const;
    QVariantList removableDevices() const;
    int screenCount() const;
    void setScreenCount(int count);

public Q_SLOTS:
    Q_SCRIPTABLE void requestWirelessScan();
    Q_SCRIPTABLE void setNetworkingEnabled(bool enabled);
    Q_SCRIPTABLE void setWirelessEnabled(bool enabled);
    Q_SCRIPTABLE void connectNetwork(const QString &device, const QString &accessPoint,
                                    const QString &ssid, const QString &savedConnection,
                                    const QString &password);
    Q_SCRIPTABLE void disconnectNetwork(const QString &device);
    Q_SCRIPTABLE void setVolume(int percent);
    Q_SCRIPTABLE void toggleMute();
    Q_SCRIPTABLE void setDefaultOutput(int index);
    Q_SCRIPTABLE void setDefaultInput(int index);
    Q_SCRIPTABLE void setStreamVolume(int index, int percent);
    Q_SCRIPTABLE void setStreamMuted(int index, bool muted);
    Q_SCRIPTABLE void activateStatusItem(const QString &service, const QString &path, int x, int y);
    Q_SCRIPTABLE void unmountRemovable(const QString &udi);
    Q_SCRIPTABLE void openControlPanel(const QString &setting);
    Q_SCRIPTABLE void toggleNotifications(const QString &screenName);
    Q_SCRIPTABLE QString dumpState() const;

Q_SIGNALS:
    void networkChanged();
    void audioChanged();
    void powerChanged();
    void statusItemsChanged();
    void removableDevicesChanged();
    void screenCountChanged();
    void operationError(const QString &title, const QString &message);

private:
    void refreshNetwork();
    void refreshAudio();
    void refreshPower();
    void refreshStatusItems();
    void refreshRemovableDevices();
    PulseAudioQt::Sink *defaultSink() const;

    QVariantList m_networks;
    QVariantList m_outputs;
    QVariantList m_inputs;
    QVariantList m_streams;
    QVariantList m_statusItems;
    QVariantList m_removableDevices;
    QString m_networkStatus;
    bool m_networkingEnabled = false;
    bool m_wirelessEnabled = false;
    bool m_batteryPresent = false;
    double m_batteryPercentage = 0;
    bool m_batteryCharging = false;
    PulseAudioQt::Context *m_audio = nullptr;
    int m_screenCount = 0;
    bool m_networkUnavailableForTest = false;
    bool m_audioUnavailableForTest = false;
};
