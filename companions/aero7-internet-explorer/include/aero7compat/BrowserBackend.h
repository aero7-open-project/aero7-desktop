#pragma once

#include <QString>

namespace Aero7::Compat {

struct BrowserBackend {
    QString displayName;
    QString desktopId;
    QString desktopFilePath;
    QString iconName;
    QString newWindowAction;
    QString privateAction;

    [[nodiscard]] bool isValid() const
    {
        return !displayName.isEmpty() && !desktopId.isEmpty()
            && !desktopFilePath.isEmpty();
    }
    [[nodiscard]] bool supportsNewWindow() const
    {
        return !newWindowAction.isEmpty();
    }
    [[nodiscard]] bool supportsPrivateMode() const
    {
        return !privateAction.isEmpty();
    }
};

} // namespace Aero7::Compat
