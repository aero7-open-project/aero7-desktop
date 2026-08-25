#pragma once

#include "BrowserBackend.h"

#include <QList>
#include <QStringList>

namespace Aero7::Compat {

class BrowserRegistry {
public:
    explicit BrowserRegistry(QStringList applicationDirectories = {});

    [[nodiscard]] QList<BrowserBackend> installedBrowsers() const;
    [[nodiscard]] BrowserBackend find(const QString &desktopId) const;
    [[nodiscard]] QStringList applicationDirectories() const;

    [[nodiscard]] static QStringList defaultApplicationDirectories();
    [[nodiscard]] static bool isSafeDesktopId(const QString &desktopId);
    [[nodiscard]] static bool isWrapperDesktopId(const QString &desktopId);

private:
    [[nodiscard]] BrowserBackend readDesktopFile(
        const QString &path, const QString &desktopId) const;
    [[nodiscard]] static QString desktopIdForPath(
        const QString &root, const QString &path);

    QStringList m_applicationDirectories;
};

} // namespace Aero7::Compat
