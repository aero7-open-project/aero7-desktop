// SPDX-License-Identifier: MIT
#include "ControlPanelController.h"

#include <QDBusConnection>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QTextStream>

int main(int argc, char **argv)
{
    for (int index = 1; index < argc; ++index) {
        const QByteArray argument(argv[index]);
        if (argument == "--list-settings-json") {
            qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
            break;
        }
    }
    QGuiApplication application(argc, argv);
    application.setApplicationName(QStringLiteral("Aero7 Control Panel"));
    application.setDesktopFileName(QStringLiteral("org.aero7.controlpanel"));

    ControlPanelController controller;
    const auto arguments = application.arguments();
    if (arguments.contains(QStringLiteral("--list-settings-json"))) {
        QTextStream(stdout) << controller.settingsJson() << Qt::endl;
        return 0;
    }
    const auto settingIndex = arguments.indexOf(QStringLiteral("--setting"));
    if (settingIndex >= 0 && settingIndex + 1 < arguments.size()) {
        const auto setting = arguments.at(settingIndex + 1);
        if (!controller.launchSetting(setting)) return 1;
        if (!QStringList{QStringLiteral("display"), QStringLiteral("default-apps"),
                         QStringLiteral("file-associations"), QStringLiteral("personalization"),
                         QStringLiteral("wallpaper"), QStringLiteral("gadgets")}.contains(setting)) return 0;
    }

    auto bus = QDBusConnection::sessionBus();
    bus.registerService(QStringLiteral("org.aero7.ControlPanel"));
    bus.registerObject(QStringLiteral("/ControlPanel"), &controller,
                       QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals
                           | QDBusConnection::ExportAllProperties | QDBusConnection::ExportScriptableInvokables);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("controlPanel"), &controller);
    engine.load(QUrl(QStringLiteral("qrc:/aero7/controlpanel/ControlPanelView.qml")));
    if (engine.rootObjects().isEmpty()) return 1;
    return application.exec();
}
