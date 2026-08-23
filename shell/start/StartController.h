// SPDX-License-Identifier: MIT
#pragma once

#include <QObject>

class ApplicationsModel;

class StartController final : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.aero7.Start")
    Q_PROPERTY(QObject *applications READ applications CONSTANT)
    Q_PROPERTY(QString userName READ userName CONSTANT)
    Q_PROPERTY(QString userIcon READ userIcon CONSTANT)
    Q_PROPERTY(QString hostName READ hostName CONSTANT)
    Q_PROPERTY(bool canSuspend READ canSuspend CONSTANT)
    Q_PROPERTY(bool canHibernate READ canHibernate CONSTANT)
    Q_PROPERTY(bool visible READ visible WRITE setVisible NOTIFY visibleChanged)
    Q_PROPERTY(QString screenName READ screenName WRITE setScreenName NOTIFY screenNameChanged)

public:
    explicit StartController(QObject *parent = nullptr);
    [[nodiscard]] QObject *applications() const;
    [[nodiscard]] QString userName() const;
    [[nodiscard]] QString userIcon() const;
    [[nodiscard]] QString hostName() const;
    [[nodiscard]] bool canSuspend() const;
    [[nodiscard]] bool canHibernate() const;
    [[nodiscard]] bool visible() const;
    void setVisible(bool visible);
    [[nodiscard]] QString screenName() const;
    void setScreenName(const QString &name);

    Q_INVOKABLE void openLocation(const QString &location);
    Q_INVOKABLE void openControlPanel();
    Q_INVOKABLE void openControlPanelSetting(const QString &setting);
    Q_INVOKABLE void openDevicesAndPrinters();
    Q_INVOKABLE void lockSession();
    Q_INVOKABLE void requestPowerAction(const QString &action);

public Q_SLOTS:
    Q_SCRIPTABLE void toggle();
    Q_SCRIPTABLE void toggleOnScreen(const QString &requestedScreen);
    Q_SCRIPTABLE void hide();
    Q_SCRIPTABLE void setSearchQuery(const QString &query);
    Q_SCRIPTABLE bool launchStorageId(const QString &storageId);
    Q_SCRIPTABLE QString dumpState() const;

Q_SIGNALS:
    void visibleChanged();
    void screenNameChanged();
    void toggleRequested(const QString &screenName);
    void hideRequested();
    void operationError(const QString &title, const QString &message);

private:
    ApplicationsModel *m_applications;
    bool m_visible = false;
    QString m_screenName;
};
