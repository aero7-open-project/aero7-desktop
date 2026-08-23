#include "GadgetManager.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QJsonDocument>
#include <QSet>
#include <QStandardPaths>

namespace {
constexpr auto serviceName = "org.aero7.Gadgets";
constexpr auto objectPath = "/GadgetManager";
}

int main(int argc, char **argv)
{
    const bool galleryAlias = QFileInfo(QString::fromLocal8Bit(argv[0])).fileName()
        == QStringLiteral("aero7-gadget-gallery");
    QApplication application(argc, argv);
    QApplication::setApplicationName(QStringLiteral("aero7-gadget-host"));
    QApplication::setApplicationDisplayName(QStringLiteral("Desktop Gadgets"));
    QApplication::setDesktopFileName(QStringLiteral("org.aero7.GadgetHost"));
    QApplication::setOrganizationName(QStringLiteral("Aero7"));
    QApplication::setQuitOnLastWindowClosed(false);

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Aero7 native Desktop Gadgets runtime"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({QStringLiteral("gallery"), QStringLiteral("Open the Desktop Gadget Gallery")});
    parser.addOption({QStringLiteral("add"), QStringLiteral("Add a gadget by id"), QStringLiteral("id")});
    parser.addOption({QStringLiteral("self-test"), QStringLiteral("Validate definitions and writable XDG paths, then exit")});
    parser.process(application);
    const bool galleryRequested = galleryAlias || parser.isSet(QStringLiteral("gallery"));

    if (parser.isSet(QStringLiteral("self-test"))) {
        const auto definitions = GadgetManager::builtinDefinitions();
        if (definitions.size() != 9) {
            return 2;
        }
        const QStringList expectedNames{QStringLiteral("Calendar"), QStringLiteral("Clock"), QStringLiteral("CPU Meter"),
                                        QStringLiteral("Currency"), QStringLiteral("Feed Headlines"), QStringLiteral("Picture Puzzle"),
                                        QStringLiteral("Slide Show"), QStringLiteral("Weather"), QStringLiteral("Media Center")};
        QSet<QString> ids;
        for (int i = 0; i < definitions.size(); ++i) {
            const auto &definition = definitions.at(i);
            if (definition.name != expectedNames.at(i) || !definition.id.startsWith(QStringLiteral("org.aero7.gadgets."))
                || ids.contains(definition.id) || definition.smallSize.isEmpty()
                || (definition.supportsLarge && definition.largeSize.isEmpty())) return 4;
            ids.insert(definition.id);
            GadgetState state;
            state.id = definition.id; state.instance = QStringLiteral("test-instance"); state.monitor = QStringLiteral("test-screen");
            state.x = 173; state.y = 41; state.size = definition.supportsLarge ? QStringLiteral("large") : QStringLiteral("small");
            state.opacity = 60; state.alwaysOnTop = true; state.settings = {{QStringLiteral("test"), QStringLiteral("value")}};
            const GadgetState decoded = stateFromJson(stateToJson(state));
            if (decoded.id != state.id || decoded.instance != state.instance || decoded.monitor != state.monitor
                || decoded.x != state.x || decoded.y != state.y || decoded.size != state.size
                || decoded.opacity != state.opacity || decoded.alwaysOnTop != state.alwaysOnTop
                || decoded.settings != state.settings) return 5;
        }
        QString metadataRoot = QStringLiteral(AERO7_GADGET_METADATA_DIR);
        if (!QFileInfo::exists(metadataRoot)) {
            metadataRoot = QStandardPaths::locate(QStandardPaths::GenericDataLocation,
                                                   QStringLiteral("aero7/gadgets"), QStandardPaths::LocateDirectory);
        }
        const QStringList manifests = QDir(metadataRoot).entryList({QStringLiteral("*.json")}, QDir::Files, QDir::Name);
        if (manifests.size() != definitions.size()) return 6;
        QSet<QString> manifestIds;
        for (const QString &manifest : manifests) {
            QFile file(metadataRoot + QLatin1Char('/') + manifest);
            if (!file.open(QIODevice::ReadOnly)) return 7;
            const QJsonObject metadata = QJsonDocument::fromJson(file.readAll()).object();
            const QString id = metadata.value(QStringLiteral("id")).toString();
            if (!ids.contains(id) || manifestIds.contains(id) || metadata.value(QStringLiteral("version")).toString().isEmpty()
                || !metadata.value(QStringLiteral("smallSize")).isArray() || !metadata.value(QStringLiteral("permissions")).isArray()) return 8;
            manifestIds.insert(id);
        }
        const QString configRoot = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
            + QStringLiteral("/aero7/gadgets");
        const QString cacheRoot = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation)
            + QStringLiteral("/aero7/gadgets");
        return QDir().mkpath(configRoot) && QDir().mkpath(cacheRoot)
            && QFileInfo(configRoot).isWritable() && QFileInfo(cacheRoot).isWritable() ? 0 : 3;
    }

    auto bus = QDBusConnection::sessionBus();
    if (!bus.registerService(QString::fromLatin1(serviceName))) {
        QDBusInterface existing(QString::fromLatin1(serviceName), QString::fromLatin1(objectPath),
                                QStringLiteral("org.aero7.GadgetManager"), bus);
        if (parser.isSet(QStringLiteral("add"))) {
            existing.call(QStringLiteral("AddGadget"), parser.value(QStringLiteral("add")));
        } else if (galleryRequested) {
            existing.call(QStringLiteral("ShowGallery"));
        }
        return 0;
    }

    GadgetManager manager;
    bus.registerObject(QString::fromLatin1(objectPath), &manager,
                       QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals);
    manager.restoreSession();
    if (parser.isSet(QStringLiteral("add"))) {
        manager.AddGadget(parser.value(QStringLiteral("add")));
    }
    if (galleryRequested) {
        manager.ShowGallery();
    }
    return application.exec();
}
