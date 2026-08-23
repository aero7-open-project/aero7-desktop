// SPDX-License-Identifier: MIT
#pragma once

#include <QAbstractListModel>
#include <QFileSystemWatcher>
#include <QObject>
#include <QTimer>
#include <QUrl>
#include <QVariantList>

class DesktopItemsModel final : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles { NameRole = Qt::UserRole + 1, UrlRole, IconRole, KindRole };
    explicit DesktopItemsModel(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    Q_INVOKABLE void reload();
    QString desktopPath() const;

private:
    QVariantList m_rows;
    QFileSystemWatcher m_watcher;
};

class DesktopController final : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.aero7.Desktop")
    Q_PROPERTY(QObject *itemsModel READ itemsModel CONSTANT)
    Q_PROPERTY(QString wallpaper READ wallpaper NOTIFY wallpaperChanged)
    Q_PROPERTY(QVariantList gadgets READ gadgets NOTIFY gadgetsChanged)
    Q_PROPERTY(double cpuUsage READ cpuUsage NOTIFY cpuUsageChanged)
    Q_PROPERTY(int screenCount READ screenCount WRITE setScreenCount NOTIFY screenCountChanged)

public:
    explicit DesktopController(QObject *parent = nullptr);
    QObject *itemsModel();
    QString wallpaper() const;
    QVariantList gadgets() const;
    double cpuUsage() const;
    int screenCount() const;
    void setScreenCount(int count);

public Q_SLOTS:
    Q_SCRIPTABLE void refresh();
    Q_SCRIPTABLE bool open(const QString &url);
    Q_SCRIPTABLE bool createFolder(const QString &name);
    Q_SCRIPTABLE bool createFile(const QString &name);
    Q_SCRIPTABLE bool copyToDesktop(const QStringList &urls);
    Q_SCRIPTABLE bool renameItem(const QString &url, const QString &newName);
    Q_SCRIPTABLE bool moveToTrash(const QString &url);
    Q_SCRIPTABLE bool setWallpaper(const QString &url);
    Q_SCRIPTABLE QString addGadget(const QString &type, const QString &screenName = {});
    Q_SCRIPTABLE bool removeGadget(const QString &id);
    Q_SCRIPTABLE bool setGadgetPosition(const QString &id, int x, int y, const QString &screenName = {});
    Q_SCRIPTABLE bool setGadgetNote(const QString &id, const QString &note);
    Q_SCRIPTABLE void openControlPanel(const QString &setting);
    Q_SCRIPTABLE QString dumpState() const;

Q_SIGNALS:
    void wallpaperChanged();
    void gadgetsChanged();
    void cpuUsageChanged();
    void screenCountChanged();
    void operationError(const QString &title, const QString &message);

private:
    [[nodiscard]] bool saveConfiguration();
    void sampleCpuUsage();

    DesktopItemsModel m_items;
    QString m_configPath;
    QString m_wallpaper;
    QVariantList m_gadgets;
    QTimer m_cpuTimer;
    quint64 m_previousCpuTotal = 0;
    quint64 m_previousCpuIdle = 0;
    double m_cpuUsage = 0.0;
    int m_screenCount = 0;
};
