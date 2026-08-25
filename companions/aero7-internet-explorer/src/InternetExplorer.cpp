#include <aero7compat/InternetExplorer.h>

#include <KIO/ApplicationLauncherJob>
#include <KService>
#include <KServiceAction>

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>

namespace Aero7::Compat {

InternetExplorer::InternetExplorer(BrowserRegistry registry,
                                   BrowserConfig config,
                                   QString xdgMimeExecutable)
    : m_registry(std::move(registry))
    , m_config(std::move(config))
    , m_defaults(m_config, std::move(xdgMimeExecutable))
{
}

QList<BrowserBackend> InternetExplorer::installedBrowsers() const
{
    return m_registry.installedBrowsers();
}

BackendResolution InternetExplorer::resolveBackend(bool rememberAdopted)
{
    BackendResolution result;
    result.candidates = installedBrowsers();

    const QString configured = m_config.backendId();
    if (!configured.isEmpty()) {
        result.backend = m_registry.find(configured);
        if (result.backend.isValid()) {
            result.status = ResolutionStatus::Ready;
            m_config.rememberLastKnownBackend(configured);
            return result;
        }
        result.status = ResolutionStatus::ConfiguredBackendMissing;
        result.missingDesktopId = configured;
        return result;
    }

    const QString systemDefault = m_defaults.currentDefault(
        QStringLiteral("x-scheme-handler/http"));
    if (!systemDefault.isEmpty()
        && !BrowserRegistry::isWrapperDesktopId(systemDefault)) {
        result.backend = m_registry.find(systemDefault);
        if (result.backend.isValid()) {
            result.status = ResolutionStatus::Ready;
            result.adoptedSystemDefault = true;
            if (rememberAdopted) {
                m_config.setBackendId(systemDefault);
            }
            return result;
        }
    }

    if (result.candidates.size() == 1) {
        result.status = ResolutionStatus::Ready;
        result.backend = result.candidates.first();
        if (rememberAdopted) {
            m_config.setBackendId(result.backend.desktopId);
        }
    } else if (result.candidates.isEmpty()) {
        result.status = ResolutionStatus::NoBrowserInstalled;
    } else {
        result.status = ResolutionStatus::SelectionRequired;
    }
    return result;
}

BrowserBackend InternetExplorer::selectedBackend() const
{
    return m_registry.find(m_config.backendId());
}

bool InternetExplorer::setBackend(const QString &desktopId, QString *error)
{
    if (!BrowserRegistry::isSafeDesktopId(desktopId)
        || BrowserRegistry::isWrapperDesktopId(desktopId)) {
        if (error) *error = QStringLiteral("The selected browser ID is invalid.");
        return false;
    }
    if (!m_registry.find(desktopId).isValid()) {
        if (error) *error = QStringLiteral("The selected browser is not installed.");
        return false;
    }
    if (!m_config.setBackendId(desktopId)) {
        if (error) {
            *error = m_config.policyLocksBackend()
                ? QStringLiteral("An administrator policy locks the browser selection.")
                : QStringLiteral("The browser preference could not be saved.");
        }
        return false;
    }
    return true;
}

DefaultApplications &InternetExplorer::defaultApplications() { return m_defaults; }
const BrowserConfig &InternetExplorer::config() const { return m_config; }

QList<QUrl> InternetExplorer::normalizeUrls(const QStringList &arguments)
{
    QList<QUrl> urls;
    for (const QString &argument : arguments) {
        if (argument.isEmpty()) {
            continue;
        }
        QUrl url;
        const QByteArray encoded = argument.toUtf8();
        const QUrl encodedUrl = QUrl::fromEncoded(encoded, QUrl::StrictMode);
        if (encodedUrl.isValid() && !encodedUrl.scheme().isEmpty()) {
            url = encodedUrl;
        } else {
            const QFileInfo file(argument);
            if (file.exists() || argument.contains(QLatin1Char('/'))
                || argument.endsWith(QStringLiteral(".html"), Qt::CaseInsensitive)
                || argument.endsWith(QStringLiteral(".htm"), Qt::CaseInsensitive)) {
                url = QUrl::fromLocalFile(file.absoluteFilePath());
            } else {
                url = QUrl::fromUserInput(argument);
            }
        }
        if (url.isValid()) {
            urls.append(url);
        }
    }
    return urls;
}

QString InternetExplorer::launchModeName(LaunchMode mode)
{
    switch (mode) {
    case LaunchMode::Normal: return QStringLiteral("normal");
    case LaunchMode::NewWindow: return QStringLiteral("new-window");
    case LaunchMode::PrivateWindow: return QStringLiteral("private-window");
    }
    return QStringLiteral("normal");
}

LaunchResult InternetExplorer::launch(const QStringList &arguments,
                                      LaunchMode mode)
{
    const BackendResolution resolution = resolveBackend();
    if (resolution.status != ResolutionStatus::Ready) {
        return {false, QStringLiteral("A browser selection is required."), {}};
    }
    return launch(resolution.backend, arguments, mode);
}

LaunchResult InternetExplorer::launch(const BrowserBackend &backend,
                                      const QStringList &arguments,
                                      LaunchMode mode)
{
    LaunchResult result;
    if (!backend.isValid() || BrowserRegistry::isWrapperDesktopId(backend.desktopId)) {
        result.error = QStringLiteral("The configured browser is invalid.");
        return result;
    }
    const QList<QUrl> urls = normalizeUrls(arguments);

    const QString capturePath = qEnvironmentVariable("AERO7_IE_CAPTURE_LAUNCH");
    if (!capturePath.isEmpty()) {
        QJsonArray encodedUrls;
        for (const QUrl &url : urls) encodedUrls.append(url.toString(QUrl::FullyEncoded));
        QSaveFile capture(capturePath);
        if (!capture.open(QIODevice::WriteOnly)) {
            result.error = QStringLiteral("The test launch record could not be opened.");
            return result;
        }
        capture.write(QJsonDocument(QJsonObject{
            {QStringLiteral("desktopId"), backend.desktopId},
            {QStringLiteral("mode"), launchModeName(mode)},
            {QStringLiteral("urls"), encodedUrls},
        }).toJson(QJsonDocument::Indented));
        result.started = capture.commit();
        if (!result.started) result.error = QStringLiteral("The test launch record could not be saved.");
        recordLaunch(backend, mode, urls, result);
        return result;
    }

    KService::Ptr service = KService::serviceByDesktopPath(backend.desktopFilePath);
    if (!service) {
        service = KService::serviceByStorageId(backend.desktopId);
    }
    if (!service && QFileInfo::exists(backend.desktopFilePath)) {
        service = KService::Ptr(new KService(backend.desktopFilePath));
    }
    if (!service || !service->isValid()) {
        result.error = QStringLiteral("The configured browser desktop entry is no longer available.");
        recordLaunch(backend, mode, urls, result);
        return result;
    }

    KIO::ApplicationLauncherJob *job = nullptr;
    KService::Ptr actionService;
    QString requestedAction;
    if (mode == LaunchMode::NewWindow) requestedAction = backend.newWindowAction;
    if (mode == LaunchMode::PrivateWindow) requestedAction = backend.privateAction;
    if (!requestedAction.isEmpty()) {
        for (const KServiceAction &action : service->actions()) {
            if (action.name() == requestedAction && !action.noDisplay()) {
                const QString actionExec = action.exec();
                static const QRegularExpression urlFieldCode(
                    QStringLiteral(R"(%[fFuU])"));
                if (!urls.isEmpty() && !urlFieldCode.match(actionExec).hasMatch()) {
                    // Some browser desktop actions (notably Chromium's
                    // new-private-window action) omit a URL placeholder. KIO
                    // otherwise sends the URL to a second default application,
                    // which can expose File Explorer instead of the browser.
                    actionService = KService::Ptr(new KService(
                        service->name(), actionExec + QStringLiteral(" %U"),
                        service->icon()));
                    job = new KIO::ApplicationLauncherJob(actionService);
                } else {
                    job = new KIO::ApplicationLauncherJob(action);
                }
                break;
            }
        }
    }
    if (!job) {
        job = new KIO::ApplicationLauncherJob(service);
    }
    job->setUrls(urls);
    const QByteArray activationToken = qgetenv("XDG_ACTIVATION_TOKEN");
    if (!activationToken.isEmpty()) {
        job->setStartupId(activationToken);
    }

    QEventLoop loop;
    QObject::connect(job, &KJob::result, &loop, &QEventLoop::quit);
    job->start();
    loop.exec();
    if (job->error() == 0) {
        result.started = true;
        result.pids = job->pids();
    } else {
        result.error = job->errorText().isEmpty()
            ? QStringLiteral("The browser could not be started.") : job->errorText();
    }
    recordLaunch(backend, mode, urls, result);
    return result;
}

void InternetExplorer::recordLaunch(const BrowserBackend &backend,
                                    LaunchMode mode, const QList<QUrl> &urls,
                                    const LaunchResult &result) const
{
    const QString stateRoot = QStandardPaths::writableLocation(
        QStandardPaths::GenericStateLocation)
        + QStringLiteral("/aero7/internet-explorer");
    QDir().mkpath(stateRoot);
    QJsonArray schemes;
    for (const QUrl &url : urls) schemes.append(url.scheme());
    QJsonArray pids;
    for (qint64 pid : result.pids) pids.append(pid);
    QSaveFile file(stateRoot + QStringLiteral("/last-launch.json"));
    if (!file.open(QIODevice::WriteOnly)) return;
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    file.write(QJsonDocument(QJsonObject{
        {QStringLiteral("schema"), 1},
        {QStringLiteral("timestamp"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
        {QStringLiteral("backendDesktopId"), backend.desktopId},
        {QStringLiteral("mode"), launchModeName(mode)},
        {QStringLiteral("urlCount"), urls.size()},
        {QStringLiteral("urlSchemes"), schemes},
        {QStringLiteral("activationTokenPresent"), !qgetenv("XDG_ACTIVATION_TOKEN").isEmpty()},
        {QStringLiteral("started"), result.started},
        {QStringLiteral("pids"), pids},
    }).toJson(QJsonDocument::Indented));
    file.commit();
    qInfo("Aero7 Internet Explorer launch: backend=%s mode=%s urls=%lld started=%s",
          qPrintable(backend.desktopId), qPrintable(launchModeName(mode)),
          static_cast<long long>(urls.size()), result.started ? "true" : "false");
}

bool InternetExplorer::createDesktopShortcut(QString *error) const
{
    if (error) error->clear();
    const QString source = QStandardPaths::locate(
        QStandardPaths::ApplicationsLocation,
        QStringLiteral("aero7-internet-explorer.desktop"));
    const QString desktop = QStandardPaths::writableLocation(
        QStandardPaths::DesktopLocation);
    if (source.isEmpty() || desktop.isEmpty() || !QDir().mkpath(desktop)) {
        if (error) *error = QStringLiteral("The Internet Explorer launcher or Desktop folder could not be found.");
        return false;
    }
    const QString destination = QDir(desktop).filePath(
        QStringLiteral("Internet Explorer.desktop"));
    if (QFileInfo::exists(destination)) {
        return true;
    }
    if (!QFile::copy(source, destination)) {
        if (error) *error = QStringLiteral("The desktop shortcut could not be created.");
        return false;
    }
    QFile shortcut(destination);
    shortcut.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner
                            | QFileDevice::ExeOwner | QFileDevice::ReadGroup
                            | QFileDevice::ExeGroup | QFileDevice::ReadOther
                            | QFileDevice::ExeOther);
    return true;
}

bool InternetExplorer::openProgramsCenter()
{
    return QProcess::startDetached(QStringLiteral("aero7-programs-center"),
                                   {QStringLiteral("--search"),
                                    QStringLiteral("web browser")});
}

} // namespace Aero7::Compat
