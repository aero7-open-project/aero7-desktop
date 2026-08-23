// SPDX-License-Identifier: MIT
#include "RecoveryController.h"

#include <QCommandLineParser>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("aero7-recovery-ui"));
    app.setApplicationDisplayName(QStringLiteral("Aero7 Desktop Recovery"));
    QCommandLineParser parser;
    parser.addHelpOption();
    QCommandLineOption component({QStringLiteral("c"), QStringLiteral("component")},
                                 QStringLiteral("Failed component"), QStringLiteral("name"),
                                 QStringLiteral("AeroShell"));
    parser.addOption(component);
    parser.process(app);
    RecoveryController controller(parser.value(component));
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("recoveryController"), &controller);
    engine.load(QUrl(QStringLiteral("qrc:/aero7/recovery/RecoveryView.qml")));
    return engine.rootObjects().isEmpty() ? 70 : app.exec();
}
