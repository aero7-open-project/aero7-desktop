// SPDX-License-Identifier: MIT
#include "StartController.h"

#include <LayerShellQt/Window>

#include <QCursor>
#include <QDBusConnection>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QScreen>

int main(int argc, char **argv)
{
    QGuiApplication application(argc, argv);
    application.setApplicationName(QStringLiteral("aero7-start"));
    application.setApplicationDisplayName(QStringLiteral("Aero7 Start Menu"));
    application.setDesktopFileName(QStringLiteral("org.aero7.start"));
    application.setOrganizationName(QStringLiteral("Aero7"));

    auto bus = QDBusConnection::sessionBus();
    if (!bus.registerService(QStringLiteral("org.aero7.Start"))) {
        qCritical() << "Another Aero7 Start menu instance is already active";
        return 73;
    }

    StartController controller;
    bus.registerObject(QStringLiteral("/Start"), &controller,
                       QDBusConnection::ExportAllSignals | QDBusConnection::ExportAllProperties
                           | QDBusConnection::ExportScriptableSlots);

    QQmlEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("startController"), &controller);
    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/aero7/start/StartView.qml")));
    if (component.isError()) {
        qCritical().noquote() << component.errorString();
        return 70;
    }
    auto *window = qobject_cast<QQuickWindow *>(component.create(engine.rootContext()));
    if (!window) {
        qCritical().noquote() << component.errorString();
        return 70;
    }

    auto *layer = LayerShellQt::Window::get(window);
    LayerShellQt::Window::Anchors anchors;
    anchors.setFlag(LayerShellQt::Window::AnchorBottom);
    anchors.setFlag(LayerShellQt::Window::AnchorLeft);
    layer->setAnchors(anchors);
    layer->setScope(QStringLiteral("aero7-start"));
    layer->setExclusiveZone(0);
    layer->setMargins(QMargins(0, 0, 0, 48));
    layer->setDesiredSize(QSize(520, 590));
    layer->setLayer(LayerShellQt::Window::LayerOverlay);
    layer->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityExclusive);
    layer->setActivateOnShow(true);
    window->hide();

    QObject::connect(&controller, &StartController::toggleRequested, window, [window, layer, &controller](const QString &screenName) {
        if (window->isVisible()) {
            window->hide();
            controller.setVisible(false);
            return;
        }
        QScreen *target = nullptr;
        for (auto *screen : QGuiApplication::screens()) {
            if (screen->name() == screenName) {
                target = screen;
                break;
            }
        }
        if (!target) target = QGuiApplication::screenAt(QCursor::pos());
        if (!target) target = QGuiApplication::primaryScreen();
        window->setScreen(target);
        layer->setScreen(target);
        controller.setScreenName(target ? target->name() : QString{});
        window->show();
        window->requestActivate();
        controller.setVisible(true);
    });
    QObject::connect(&controller, &StartController::hideRequested, window, [window, &controller]() {
        window->hide();
        controller.setVisible(false);
    });

    return application.exec();
}
