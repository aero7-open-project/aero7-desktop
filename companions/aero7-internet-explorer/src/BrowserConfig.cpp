#include <aero7compat/BrowserConfig.h>
#include <aero7compat/BrowserRegistry.h>

#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>

namespace Aero7::Compat {

BrowserConfig::BrowserConfig(QString userPath, QString policyPath)
    : m_userPath(userPath.isEmpty() ? defaultUserPath() : std::move(userPath))
    , m_policyPath(policyPath.isEmpty() ? defaultPolicyPath() : std::move(policyPath))
{
}

QString BrowserConfig::defaultUserPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
        + QStringLiteral("/aero7/internet-explorer.conf");
}

QString BrowserConfig::defaultPolicyPath()
{
    return QStringLiteral("/etc/aero7/internet-explorer.conf");
}

QString BrowserConfig::userBackendId() const
{
    QSettings settings(m_userPath, QSettings::IniFormat);
    return settings.value(QStringLiteral("Browser/Backend")).toString().trimmed();
}

QString BrowserConfig::backendId() const
{
    QSettings policy(m_policyPath, QSettings::IniFormat);
    const QString enforced = policy.value(QStringLiteral("Policy/Backend"))
                                 .toString().trimmed();
    if (BrowserRegistry::isSafeDesktopId(enforced)
        && !BrowserRegistry::isWrapperDesktopId(enforced)) {
        return enforced;
    }
    return userBackendId();
}

QString BrowserConfig::lastKnownBackendId() const
{
    QSettings settings(m_userPath, QSettings::IniFormat);
    return settings.value(QStringLiteral("Browser/LastKnownBackend"))
        .toString().trimmed();
}

bool BrowserConfig::policyLocksBackend() const
{
    QSettings policy(m_policyPath, QSettings::IniFormat);
    const QString enforced = policy.value(QStringLiteral("Policy/Backend"))
                                 .toString().trimmed();
    return BrowserRegistry::isSafeDesktopId(enforced)
        && !BrowserRegistry::isWrapperDesktopId(enforced)
        && !policy.value(QStringLiteral("Policy/AllowUserChange"), true).toBool();
}

bool BrowserConfig::mayChangeBackend() const
{
    return !policyLocksBackend();
}

bool BrowserConfig::setBackendId(const QString &desktopId)
{
    if (!mayChangeBackend() || !BrowserRegistry::isSafeDesktopId(desktopId)
        || BrowserRegistry::isWrapperDesktopId(desktopId)) {
        return false;
    }
    QDir().mkpath(QFileInfo(m_userPath).absolutePath());
    QSettings settings(m_userPath, QSettings::IniFormat);
    settings.setValue(QStringLiteral("Browser/Backend"), desktopId);
    settings.setValue(QStringLiteral("Browser/LastKnownBackend"), desktopId);
    settings.sync();
    return settings.status() == QSettings::NoError;
}

void BrowserConfig::rememberLastKnownBackend(const QString &desktopId)
{
    if (!BrowserRegistry::isSafeDesktopId(desktopId)
        || BrowserRegistry::isWrapperDesktopId(desktopId)) {
        return;
    }
    QDir().mkpath(QFileInfo(m_userPath).absolutePath());
    QSettings settings(m_userPath, QSettings::IniFormat);
    settings.setValue(QStringLiteral("Browser/LastKnownBackend"), desktopId);
    settings.sync();
}

void BrowserConfig::setPreviousDefault(const QString &mimeType,
                                       const QString &desktopId)
{
    if (mimeType.isEmpty() || desktopId.isEmpty()
        || BrowserRegistry::isWrapperDesktopId(desktopId)) {
        return;
    }
    QDir().mkpath(QFileInfo(m_userPath).absolutePath());
    QSettings settings(m_userPath, QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("PreviousDefaults"));
    settings.setValue(QString::fromLatin1(mimeType.toUtf8().toBase64(
                          QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals)),
                      desktopId);
    settings.endGroup();
    settings.sync();
}

QString BrowserConfig::previousDefault(const QString &mimeType) const
{
    QSettings settings(m_userPath, QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("PreviousDefaults"));
    return settings.value(QString::fromLatin1(mimeType.toUtf8().toBase64(
                              QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals)))
        .toString().trimmed();
}

QString BrowserConfig::userPath() const { return m_userPath; }
QString BrowserConfig::policyPath() const { return m_policyPath; }

} // namespace Aero7::Compat
