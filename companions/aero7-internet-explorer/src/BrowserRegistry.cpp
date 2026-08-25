#include <aero7compat/BrowserRegistry.h>
#include <aero7compat/DefaultApplications.h>

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSettings>
#include <QSet>
#include <QStandardPaths>

#include <KService>
#include <KServiceAction>

#include <algorithm>

namespace Aero7::Compat {

namespace {

QStringList desktopList(const QVariant &value)
{
    if (value.metaType().id() == QMetaType::QStringList) {
        return value.toStringList();
    }
    return value.toString().split(QLatin1Char(';'), Qt::SkipEmptyParts);
}

bool isTruthy(const QVariant &value)
{
    const QString text = value.toString().trimmed().toLower();
    return value.toBool() || text == QLatin1String("true")
        || text == QLatin1String("1") || text == QLatin1String("yes");
}

QString findAction(const QSettings &settings, const QStringList &actions,
                   const QStringList &needles)
{
    for (const QString &action : actions) {
        const QString group = QStringLiteral("Desktop Action %1").arg(action);
        const QString text = settings.value(group + QStringLiteral("/Name")).toString();
        const QString haystack = (action + QLatin1Char(' ') + text).toLower();
        for (const QString &needle : needles) {
            if (haystack.contains(needle)) {
                return action;
            }
        }
    }
    return {};
}

QString findAction(const QList<KServiceAction> &actions,
                   const QStringList &needles)
{
    for (const KServiceAction &action : actions) {
        const QString haystack = (action.name() + QLatin1Char(' ')
                                  + action.text()).toLower();
        for (const QString &needle : needles) {
            if (haystack.contains(needle)) {
                return action.name();
            }
        }
    }
    return {};
}

} // namespace

BrowserRegistry::BrowserRegistry(QStringList applicationDirectories)
    : m_applicationDirectories(applicationDirectories.isEmpty()
          ? defaultApplicationDirectories() : std::move(applicationDirectories))
{
    m_applicationDirectories.removeDuplicates();
}

QStringList BrowserRegistry::defaultApplicationDirectories()
{
    QStringList directories;
    for (const QString &data : QStandardPaths::standardLocations(
             QStandardPaths::GenericDataLocation)) {
        directories.append(QDir(data).filePath(QStringLiteral("applications")));
    }
    directories.prepend(QDir::home().filePath(
        QStringLiteral(".local/share/flatpak/exports/share/applications")));
    directories.append(QStringLiteral(
        "/var/lib/flatpak/exports/share/applications"));
    directories.removeDuplicates();
    return directories;
}

bool BrowserRegistry::isSafeDesktopId(const QString &desktopId)
{
    static const QRegularExpression safe(
        QStringLiteral(R"(^[A-Za-z0-9][A-Za-z0-9._-]*\.desktop$)"));
    return desktopId.size() <= 255 && safe.match(desktopId).hasMatch();
}

bool BrowserRegistry::isWrapperDesktopId(const QString &desktopId)
{
    return desktopId.compare(QString::fromLatin1(
        DefaultApplications::WrapperDesktopId), Qt::CaseInsensitive) == 0;
}

QString BrowserRegistry::desktopIdForPath(const QString &root,
                                          const QString &path)
{
    QString relative = QDir(root).relativeFilePath(path);
    relative.replace(QLatin1Char('/'), QLatin1Char('-'));
    return relative;
}

BrowserBackend BrowserRegistry::readDesktopFile(const QString &path,
                                                const QString &desktopId) const
{
    if (!isSafeDesktopId(desktopId) || isWrapperDesktopId(desktopId)) {
        return {};
    }

    QSettings settings(path, QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("Desktop Entry"));
    if (settings.value(QStringLiteral("Type")).toString()
            != QLatin1String("Application")
        || isTruthy(settings.value(QStringLiteral("Hidden")))
        || isTruthy(settings.value(QStringLiteral("NoDisplay")))) {
        return {};
    }

    const QString name = settings.value(QStringLiteral("Name")).toString().trimmed();
    const QString exec = settings.value(QStringLiteral("Exec")).toString().trimmed();
    const QString tryExec = settings.value(QStringLiteral("TryExec")).toString().trimmed();
    const QString icon = settings.value(QStringLiteral("Icon")).toString().trimmed();
    const QStringList mimeTypes = desktopList(settings.value(QStringLiteral("MimeType")));
    const QStringList categories = desktopList(settings.value(QStringLiteral("Categories")));
    const QStringList actions = desktopList(settings.value(QStringLiteral("Actions")));
    settings.endGroup();

    if (name.isEmpty() || exec.isEmpty()) {
        return {};
    }
    if (!tryExec.isEmpty() && QStandardPaths::findExecutable(tryExec).isEmpty()) {
        return {};
    }

    const QString identity = (desktopId + QLatin1Char(' ') + name).toLower();
    const bool knownBrowser = identity.contains(QStringLiteral("firefox"))
        || identity.contains(QStringLiteral("librewolf"))
        || identity.contains(QStringLiteral("chromium"))
        || identity.contains(QStringLiteral("chrome"))
        || identity.contains(QStringLiteral("brave"))
        || identity.contains(QStringLiteral("vivaldi"))
        || identity.contains(QStringLiteral("falkon"));
    const bool hasWebMime = mimeTypes.contains(QStringLiteral("x-scheme-handler/http"))
        || mimeTypes.contains(QStringLiteral("x-scheme-handler/https"))
        || mimeTypes.contains(QStringLiteral("text/html"));
    const bool webCategory = categories.contains(QStringLiteral("WebBrowser"),
                                                  Qt::CaseInsensitive);
    if (!knownBrowser && !hasWebMime && !webCategory) {
        return {};
    }

    QSettings actionSettings(path, QSettings::IniFormat);
    QStringList effectiveActions = actions;
    for (const QString &group : actionSettings.childGroups()) {
        static const QString prefix = QStringLiteral("Desktop Action ");
        if (group.startsWith(prefix)) {
            effectiveActions.append(group.mid(prefix.size()));
        }
    }
    effectiveActions.removeDuplicates();
    const KService::Ptr service = KService::serviceByDesktopPath(path);
    const QList<KServiceAction> serviceActions = service ? service->actions()
                                                         : QList<KServiceAction>{};
    BrowserBackend backend;
    backend.displayName = name;
    backend.desktopId = desktopId;
    backend.desktopFilePath = QFileInfo(path).canonicalFilePath();
    if (backend.desktopFilePath.isEmpty()) {
        backend.desktopFilePath = QFileInfo(path).absoluteFilePath();
    }
    backend.iconName = icon.isEmpty() ? QStringLiteral("internet-web-browser") : icon;
    backend.newWindowAction = findAction(serviceActions,
        {QStringLiteral("newwindow"), QStringLiteral("new-window"),
         QStringLiteral("new window")});
    if (backend.newWindowAction.isEmpty()) {
        backend.newWindowAction = findAction(actionSettings, effectiveActions,
            {QStringLiteral("newwindow"), QStringLiteral("new-window"),
             QStringLiteral("new window")});
    }
    backend.privateAction = findAction(serviceActions,
        {QStringLiteral("private"), QStringLiteral("incognito"),
         QStringLiteral("inprivate")});
    if (backend.privateAction.isEmpty()) {
        backend.privateAction = findAction(actionSettings, effectiveActions,
            {QStringLiteral("private"), QStringLiteral("incognito"),
             QStringLiteral("inprivate")});
    }
    return backend;
}

QList<BrowserBackend> BrowserRegistry::installedBrowsers() const
{
    QList<BrowserBackend> browsers;
    QSet<QString> seen;
    for (const QString &root : m_applicationDirectories) {
        if (!QFileInfo::exists(root)) {
            continue;
        }
        QDirIterator iterator(root, {QStringLiteral("*.desktop")}, QDir::Files,
                              QDirIterator::Subdirectories);
        while (iterator.hasNext()) {
            const QString path = iterator.next();
            const QString desktopId = desktopIdForPath(root, path);
            if (seen.contains(desktopId)) {
                continue;
            }
            const BrowserBackend backend = readDesktopFile(path, desktopId);
            if (!backend.isValid()) {
                continue;
            }
            seen.insert(desktopId);
            browsers.append(backend);
        }
    }
    std::sort(browsers.begin(), browsers.end(), [](const auto &left,
                                                    const auto &right) {
        return QString::localeAwareCompare(left.displayName, right.displayName) < 0;
    });
    return browsers;
}

BrowserBackend BrowserRegistry::find(const QString &desktopId) const
{
    if (!isSafeDesktopId(desktopId) || isWrapperDesktopId(desktopId)) {
        return {};
    }
    const auto browsers = installedBrowsers();
    for (const BrowserBackend &backend : browsers) {
        if (backend.desktopId == desktopId) {
            return backend;
        }
    }
    return {};
}

QStringList BrowserRegistry::applicationDirectories() const
{
    return m_applicationDirectories;
}

} // namespace Aero7::Compat
