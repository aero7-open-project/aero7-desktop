#include <aero7compat/DefaultApplications.h>
#include <aero7compat/BrowserRegistry.h>

#include <QHash>
#include <QProcess>
#include <QStandardPaths>

namespace Aero7::Compat {

DefaultApplications::DefaultApplications(BrowserConfig &config,
                                         QString xdgMimeExecutable)
    : m_config(config)
    , m_xdgMimeExecutable(xdgMimeExecutable.isEmpty()
          ? QStandardPaths::findExecutable(QStringLiteral("xdg-mime"))
          : std::move(xdgMimeExecutable))
{
}

QStringList DefaultApplications::browserMimeTypes()
{
    return {QStringLiteral("x-scheme-handler/http"),
            QStringLiteral("x-scheme-handler/https"),
            QStringLiteral("text/html")};
}

QString DefaultApplications::currentDefault(const QString &mimeType) const
{
    if (m_xdgMimeExecutable.isEmpty() || mimeType.isEmpty()) {
        return {};
    }
    QProcess process;
    process.start(m_xdgMimeExecutable,
                  {QStringLiteral("query"), QStringLiteral("default"), mimeType});
    if (!process.waitForStarted(3000) || !process.waitForFinished(5000)
        || process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        return {};
    }
    const QString desktopId = QString::fromUtf8(process.readAllStandardOutput())
                                  .trimmed();
    return BrowserRegistry::isSafeDesktopId(desktopId) ? desktopId : QString();
}

bool DefaultApplications::isInternetExplorerDefault() const
{
    for (const QString &mimeType : browserMimeTypes()) {
        if (!BrowserRegistry::isWrapperDesktopId(currentDefault(mimeType))) {
            return false;
        }
    }
    return true;
}

bool DefaultApplications::setDefault(const QString &desktopId,
                                     const QString &mimeType,
                                     QString *error) const
{
    if (m_xdgMimeExecutable.isEmpty()) {
        if (error) *error = QStringLiteral("xdg-mime is not installed.");
        return false;
    }
    if (!BrowserRegistry::isSafeDesktopId(desktopId)) {
        if (error) *error = QStringLiteral("The desktop application ID is invalid.");
        return false;
    }
    QProcess process;
    process.start(m_xdgMimeExecutable,
                  {QStringLiteral("default"), desktopId, mimeType});
    if (!process.waitForStarted(3000) || !process.waitForFinished(5000)
        || process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        if (error) {
            const QString detail = QString::fromUtf8(process.readAllStandardError()).trimmed();
            *error = detail.isEmpty()
                ? QStringLiteral("The default application could not be changed.") : detail;
        }
        return false;
    }
    return true;
}

bool DefaultApplications::setInternetExplorerDefault(QString *error)
{
    if (error) error->clear();
    QHash<QString, QString> originalDefaults;
    for (const QString &mimeType : browserMimeTypes()) {
        const QString previous = currentDefault(mimeType);
        originalDefaults.insert(mimeType, previous);
        if (!previous.isEmpty() && !BrowserRegistry::isWrapperDesktopId(previous)) {
            m_config.setPreviousDefault(mimeType, previous);
        }
    }
    QStringList changedMimeTypes;
    for (const QString &mimeType : browserMimeTypes()) {
        if (!setDefault(QString::fromLatin1(WrapperDesktopId), mimeType, error)) {
            const QString originalError = error ? *error : QString();
            for (auto it = changedMimeTypes.crbegin(); it != changedMimeTypes.crend(); ++it) {
                const QString previous = originalDefaults.value(*it);
                if (!previous.isEmpty()) {
                    setDefault(previous, *it, nullptr);
                }
            }
            if (error) *error = originalError;
            return false;
        }
        changedMimeTypes.append(mimeType);
    }
    return true;
}

bool DefaultApplications::restorePreviousDefaults(QString *error)
{
    if (error) error->clear();
    bool changed = false;
    for (const QString &mimeType : browserMimeTypes()) {
        if (!BrowserRegistry::isWrapperDesktopId(currentDefault(mimeType))) {
            continue;
        }
        const QString previous = m_config.previousDefault(mimeType);
        if (previous.isEmpty() || BrowserRegistry::isWrapperDesktopId(previous)) {
            continue;
        }
        if (!setDefault(previous, mimeType, error)) {
            return false;
        }
        changed = true;
    }
    if (!changed && error) {
        *error = QStringLiteral("No previous browser default was recorded.");
    }
    return changed;
}

} // namespace Aero7::Compat
