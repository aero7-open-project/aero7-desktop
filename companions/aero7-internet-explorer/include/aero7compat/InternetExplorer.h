#pragma once

#include "BrowserBackend.h"
#include "BrowserConfig.h"
#include "BrowserRegistry.h"
#include "DefaultApplications.h"

#include <QList>
#include <QStringList>
#include <QUrl>

namespace Aero7::Compat {

enum class ResolutionStatus {
    Ready,
    SelectionRequired,
    ConfiguredBackendMissing,
    NoBrowserInstalled,
};

struct BackendResolution {
    ResolutionStatus status = ResolutionStatus::NoBrowserInstalled;
    BrowserBackend backend;
    QList<BrowserBackend> candidates;
    QString missingDesktopId;
    bool adoptedSystemDefault = false;
};

enum class LaunchMode {
    Normal,
    NewWindow,
    PrivateWindow,
};

struct LaunchResult {
    bool started = false;
    QString error;
    QList<qint64> pids;
};

class InternetExplorer {
public:
    explicit InternetExplorer(BrowserRegistry registry = BrowserRegistry(),
                              BrowserConfig config = BrowserConfig(),
                              QString xdgMimeExecutable = {});

    [[nodiscard]] QList<BrowserBackend> installedBrowsers() const;
    [[nodiscard]] BackendResolution resolveBackend(bool rememberAdopted = true);
    [[nodiscard]] BrowserBackend selectedBackend() const;
    bool setBackend(const QString &desktopId, QString *error = nullptr);

    [[nodiscard]] DefaultApplications &defaultApplications();
    [[nodiscard]] const BrowserConfig &config() const;

    LaunchResult launch(const QStringList &arguments,
                        LaunchMode mode = LaunchMode::Normal);
    LaunchResult launch(const BrowserBackend &backend,
                        const QStringList &arguments,
                        LaunchMode mode = LaunchMode::Normal);

    bool createDesktopShortcut(QString *error = nullptr) const;
    static bool openProgramsCenter();
    [[nodiscard]] static QList<QUrl> normalizeUrls(const QStringList &arguments);
    [[nodiscard]] static QString launchModeName(LaunchMode mode);

private:
    void recordLaunch(const BrowserBackend &backend, LaunchMode mode,
                      const QList<QUrl> &urls, const LaunchResult &result) const;

    BrowserRegistry m_registry;
    BrowserConfig m_config;
    DefaultApplications m_defaults;
};

} // namespace Aero7::Compat
