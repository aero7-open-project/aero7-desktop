#pragma once

#include "BrowserConfig.h"

#include <QString>
#include <QStringList>

namespace Aero7::Compat {

class DefaultApplications {
public:
    static constexpr auto WrapperDesktopId = "aero7-internet-explorer.desktop";

    explicit DefaultApplications(BrowserConfig &config,
                                 QString xdgMimeExecutable = {});

    [[nodiscard]] QString currentDefault(const QString &mimeType) const;
    [[nodiscard]] bool isInternetExplorerDefault() const;
    bool setInternetExplorerDefault(QString *error = nullptr);
    bool restorePreviousDefaults(QString *error = nullptr);

    [[nodiscard]] static QStringList browserMimeTypes();

private:
    bool setDefault(const QString &desktopId, const QString &mimeType,
                    QString *error) const;

    BrowserConfig &m_config;
    QString m_xdgMimeExecutable;
};

} // namespace Aero7::Compat
