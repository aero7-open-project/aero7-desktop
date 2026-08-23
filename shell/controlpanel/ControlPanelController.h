// SPDX-License-Identifier: MIT
#pragma once

#include <QObject>
#include <QJsonObject>
#include <QTimer>
#include <QVariantList>

class ControlPanelController final : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.aero7.ControlPanel")
    Q_PROPERTY(QVariantList settings READ settings NOTIFY settingsChanged)
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)
    Q_PROPERTY(QString viewMode READ viewMode WRITE setViewMode NOTIFY viewModeChanged)
    Q_PROPERTY(QString currentPage READ currentPage WRITE setCurrentPage NOTIFY currentPageChanged)
    Q_PROPERTY(QVariantList displayOutputs READ displayOutputs NOTIFY displayOutputsChanged)
    Q_PROPERTY(QString displayStatus READ displayStatus NOTIFY displayStatusChanged)
    Q_PROPERTY(bool displayChangePending READ displayChangePending NOTIFY displayChangePendingChanged)
    Q_PROPERTY(int displayRevertSeconds READ displayRevertSeconds NOTIFY displayRevertSecondsChanged)
    Q_PROPERTY(QVariantList defaultPrograms READ defaultPrograms NOTIFY defaultProgramsChanged)
    Q_PROPERTY(QString gadgetStatus READ gadgetStatus NOTIFY gadgetStatusChanged)

public:
    explicit ControlPanelController(QObject *parent = nullptr);

    [[nodiscard]] QVariantList settings() const;
    [[nodiscard]] QString query() const;
    void setQuery(const QString &query);
    [[nodiscard]] QString viewMode() const;
    void setViewMode(const QString &viewMode);
    [[nodiscard]] QString currentPage() const;
    void setCurrentPage(const QString &page);
    [[nodiscard]] QVariantList displayOutputs() const;
    [[nodiscard]] QString displayStatus() const;
    [[nodiscard]] bool displayChangePending() const;
    [[nodiscard]] int displayRevertSeconds() const;
    [[nodiscard]] QVariantList defaultPrograms() const;
    [[nodiscard]] QString gadgetStatus() const;

    [[nodiscard]] QByteArray settingsJson() const;

public Q_SLOTS:
    bool launchSetting(const QString &key);
    void refreshDisplays();
    bool applyDisplay(int outputId, const QString &modeId, const QString &rotation,
                      double scale, bool enabled, bool primary, int x, int y);
    void confirmDisplayChange();
    void revertDisplayChange();
    void refreshDefaultPrograms();
    bool setDefaultProgram(const QString &mimeType, const QString &desktopId);
    bool setWallpaper(const QString &url);
    bool addDesktopGadget(const QString &type);
    QString dumpState() const;

Q_SIGNALS:
    void settingsChanged();
    void queryChanged();
    void viewModeChanged();
    void currentPageChanged();
    void displayOutputsChanged();
    void displayStatusChanged();
    void displayChangePendingChanged();
    void displayRevertSecondsChanged();
    void defaultProgramsChanged();
    void gadgetStatusChanged();
    void operationError(const QString &title, const QString &message);

private:
    void loadSettings();
    [[nodiscard]] static QString categoryFor(const QString &key);
    [[nodiscard]] static bool launchDetached(const QString &program, const QStringList &arguments = {});
    [[nodiscard]] bool runKScreen(const QStringList &arguments, QByteArray *standardOutput = nullptr);
    void setDisplayStatus(const QString &status);
    [[nodiscard]] QStringList restoreDisplayArguments() const;

    QVariantList m_allSettings;
    QString m_query;
    QString m_viewMode = QStringLiteral("Category");
    QString m_currentPage = QStringLiteral("hub");
    QVariantList m_displayOutputs;
    QVariantList m_defaultPrograms;
    QString m_displayStatus;
    QString m_gadgetStatus;
    QJsonObject m_displaySnapshot;
    QTimer m_displayRevertTimer;
    int m_displayRevertSeconds = 0;
    bool m_backendAvailable = true;
    bool m_displayBackendAvailable = true;
};
