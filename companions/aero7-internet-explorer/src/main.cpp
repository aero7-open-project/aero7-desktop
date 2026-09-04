#include "SettingsDialog.h"
#include "IconResources.h"

#include <aero7compat/InternetExplorer.h>

#include <Aero7Qt/stylesheet.h>

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QTextStream>

using namespace Aero7::Compat;

namespace {

QJsonObject backendJson(const BrowserBackend &backend)
{
    return {
        {QStringLiteral("displayName"), backend.displayName},
        {QStringLiteral("desktopId"), backend.desktopId},
        {QStringLiteral("icon"), backend.iconName},
        {QStringLiteral("supportsNewWindow"), backend.supportsNewWindow()},
        {QStringLiteral("supportsPrivateMode"), backend.supportsPrivateMode()},
    };
}

void printJson(const QJsonValue &value)
{
    QTextStream(stdout) << QJsonDocument(value.isArray()
        ? QJsonDocument(value.toArray()) : QJsonDocument(value.toObject()))
        .toJson(QJsonDocument::Compact) << Qt::endl;
}

} // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("aero7-internet-explorer"));
    app.setApplicationDisplayName(QStringLiteral("Internet Explorer"));
    app.setOrganizationName(QStringLiteral("Aero7"));
    app.setApplicationVersion(QStringLiteral("0.1.0"));
    app.setWindowIcon(aero7OwnedIcon(QStringLiteral(":/aero7/icons/app/aero7-internet-explorer.png")));
    Aero7::applyApplicationStyle(&app);

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral(
        "Aero7 Internet Explorer compatibility launcher"));
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption listBackends(QStringLiteral("list-backends"),
        QStringLiteral("Print detected browser backends as JSON."));
    const QCommandLineOption statusJson(QStringLiteral("status-json"),
        QStringLiteral("Print backend and default-browser status as JSON."));
    const QCommandLineOption getBackend(QStringLiteral("get-backend"),
        QStringLiteral("Print the selected backend desktop ID."));
    const QCommandLineOption setBackend(QStringLiteral("set-backend"),
        QStringLiteral("Select a backend desktop ID."), QStringLiteral("desktop-id"));
    const QCommandLineOption settings(QStringLiteral("settings"),
        QStringLiteral("Open Internet Explorer settings."));
    const QCommandLineOption setDefault(QStringLiteral("set-default"),
        QStringLiteral("Make the wrapper the HTTP, HTTPS, and HTML default."));
    const QCommandLineOption restoreDefaults(QStringLiteral("restore-defaults"),
        QStringLiteral("Restore recorded browser defaults."));
    const QCommandLineOption isDefault(QStringLiteral("is-default"),
        QStringLiteral("Return success when the wrapper owns all web defaults."));
    const QCommandLineOption newWindow(QStringLiteral("new-window"),
        QStringLiteral("Open a new browser window when supported."));
    const QCommandLineOption privateWindow(QStringLiteral("private"),
        QStringLiteral("Start InPrivate Browsing when supported."));
    const QCommandLineOption desktopShortcut(QStringLiteral("create-desktop-shortcut"),
        QStringLiteral("Create a permanent Internet Explorer desktop shortcut."));
    for (const auto &option : {listBackends, statusJson, getBackend, setBackend,
                              settings, setDefault, restoreDefaults, isDefault,
                              newWindow, privateWindow, desktopShortcut}) {
        parser.addOption(option);
    }
    parser.addPositionalArgument(QStringLiteral("URLs"),
        QStringLiteral("URLs or local HTML files to open."), QStringLiteral("[URLs...]"));
    parser.process(app);

    InternetExplorer internetExplorer;

    if (parser.isSet(listBackends)) {
        QJsonArray backends;
        for (const BrowserBackend &backend : internetExplorer.installedBrowsers()) {
            backends.append(backendJson(backend));
        }
        printJson(backends);
        return 0;
    }
    if (parser.isSet(statusJson)) {
        const BackendResolution resolution = internetExplorer.resolveBackend(false);
        QJsonArray backends;
        for (const BrowserBackend &backend : resolution.candidates) {
            backends.append(backendJson(backend));
        }
        printJson(QJsonObject{
            {QStringLiteral("selectedDesktopId"), internetExplorer.config().backendId()},
            {QStringLiteral("selectedDisplayName"), resolution.backend.displayName},
            {QStringLiteral("isDefault"), internetExplorer.defaultApplications().isInternetExplorerDefault()},
            {QStringLiteral("policyLocked"), internetExplorer.config().policyLocksBackend()},
            {QStringLiteral("resolutionStatus"), static_cast<int>(resolution.status)},
            {QStringLiteral("backends"), backends},
        });
        return 0;
    }
    if (parser.isSet(getBackend)) {
        QTextStream(stdout) << internetExplorer.config().backendId() << Qt::endl;
        return internetExplorer.config().backendId().isEmpty() ? 1 : 0;
    }
    if (parser.isSet(setBackend)) {
        QString error;
        if (!internetExplorer.setBackend(parser.value(setBackend), &error)) {
            QTextStream(stderr) << error << Qt::endl;
            return 2;
        }
        return 0;
    }
    if (parser.isSet(setDefault)) {
        QString error;
        if (!internetExplorer.defaultApplications().setInternetExplorerDefault(&error)) {
            QTextStream(stderr) << error << Qt::endl;
            return 2;
        }
        return 0;
    }
    if (parser.isSet(restoreDefaults)) {
        QString error;
        if (!internetExplorer.defaultApplications().restorePreviousDefaults(&error)) {
            QTextStream(stderr) << error << Qt::endl;
            return 2;
        }
        return 0;
    }
    if (parser.isSet(isDefault)) {
        return internetExplorer.defaultApplications().isInternetExplorerDefault() ? 0 : 1;
    }
    if (parser.isSet(desktopShortcut)) {
        QString error;
        if (!internetExplorer.createDesktopShortcut(&error)) {
            QTextStream(stderr) << error << Qt::endl;
            return 2;
        }
        return 0;
    }
    if (parser.isSet(settings)) {
        const BackendResolution resolution = internetExplorer.resolveBackend(false);
        if (resolution.status == ResolutionStatus::NoBrowserInstalled) {
            return showNoBrowserDialog();
        }
        SettingsDialog dialog(internetExplorer, SettingsDialog::Reason::Settings);
        return dialog.exec() == QDialog::Accepted ? 0 : 1;
    }

    BackendResolution resolution = internetExplorer.resolveBackend();
    if (resolution.status == ResolutionStatus::NoBrowserInstalled) {
        return showNoBrowserDialog();
    }
    if (resolution.status == ResolutionStatus::SelectionRequired
        || resolution.status == ResolutionStatus::ConfiguredBackendMissing) {
        SettingsDialog dialog(internetExplorer,
            resolution.status == ResolutionStatus::ConfiguredBackendMissing
                ? SettingsDialog::Reason::BackendMissing
                : SettingsDialog::Reason::FirstLaunch,
            resolution.missingDesktopId);
        if (dialog.exec() != QDialog::Accepted) {
            return 1;
        }
        resolution = internetExplorer.resolveBackend();
    }
    if (resolution.status != ResolutionStatus::Ready) {
        QMessageBox::warning(nullptr, QStringLiteral("Internet Explorer"),
            QStringLiteral("The selected browser is unavailable."));
        return 2;
    }

    LaunchMode mode = LaunchMode::Normal;
    if (parser.isSet(newWindow)) mode = LaunchMode::NewWindow;
    if (parser.isSet(privateWindow)) mode = LaunchMode::PrivateWindow;
    const LaunchResult launch = internetExplorer.launch(
        resolution.backend, parser.positionalArguments(), mode);
    if (!launch.started) {
        QMessageBox::warning(nullptr, QStringLiteral("Internet Explorer"),
            launch.error.isEmpty() ? QStringLiteral("Unable to open the link.")
                                   : launch.error);
        return 2;
    }
    return 0;
}
