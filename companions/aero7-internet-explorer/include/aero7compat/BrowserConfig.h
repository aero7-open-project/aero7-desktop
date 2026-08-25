#pragma once

#include <QString>

namespace Aero7::Compat {

class BrowserConfig {
public:
    explicit BrowserConfig(QString userPath = {}, QString policyPath = {});

    [[nodiscard]] QString backendId() const;
    [[nodiscard]] QString userBackendId() const;
    [[nodiscard]] QString lastKnownBackendId() const;
    [[nodiscard]] bool policyLocksBackend() const;
    [[nodiscard]] bool mayChangeBackend() const;

    bool setBackendId(const QString &desktopId);
    void rememberLastKnownBackend(const QString &desktopId);

    void setPreviousDefault(const QString &mimeType, const QString &desktopId);
    [[nodiscard]] QString previousDefault(const QString &mimeType) const;

    [[nodiscard]] QString userPath() const;
    [[nodiscard]] QString policyPath() const;

    [[nodiscard]] static QString defaultUserPath();
    [[nodiscard]] static QString defaultPolicyPath();

private:
    QString m_userPath;
    QString m_policyPath;
};

} // namespace Aero7::Compat
