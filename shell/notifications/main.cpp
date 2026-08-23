// SPDX-License-Identifier: MIT
#include "NotificationServer.h"

#include <LayerShellQt/Window>

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QScreen>

namespace
{
QQuickWindow *createWindow(QQmlEngine &engine, const QUrl &url)
{
    QQmlComponent component(&engine, url);
    if (component.isError()) {
        qCritical().noquote() << component.errorString();
        return nullptr;
    }
    auto *window = qobject_cast<QQuickWindow *>(component.create(engine.rootContext()));
    if (!window) qCritical().noquote() << component.errorString();
    return window;
}

void configureLayer(QQuickWindow *window, QScreen *screen, int bottomMargin)
{
    window->setScreen(screen);
    auto *layer = LayerShellQt::Window::get(window);
    LayerShellQt::Window::Anchors anchors;
    anchors.setFlag(LayerShellQt::Window::AnchorBottom);
    anchors.setFlag(LayerShellQt::Window::AnchorRight);
    layer->setAnchors(anchors);
    layer->setScreen(screen);
    layer->setScope(QStringLiteral("aero7-notifications"));
    layer->setExclusiveZone(0);
    layer->setMargins(QMargins(0, 0, 14, bottomMargin));
    layer->setDesiredSize(window->size());
    layer->setLayer(LayerShellQt::Window::LayerOverlay);
    layer->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityOnDemand);
}
}

int main(int argc, char **argv)
{
    QGuiApplication application(argc, argv);
    application.setApplicationName(QStringLiteral("aero7-notify"));
    application.setApplicationDisplayName(QStringLiteral("Aero7 Notifications"));
    application.setDesktopFileName(QStringLiteral("org.aero7.notifications"));
    application.setOrganizationName(QStringLiteral("Aero7"));

    auto bus = QDBusConnection::sessionBus();
    if (!bus.registerService(QStringLiteral("org.aero7.Notifications"))) return 73;
    NotificationServer server;
    const auto registration = bus.interface()->registerService(
        QStringLiteral("org.freedesktop.Notifications"),
        QDBusConnectionInterface::ReplaceExistingService,
        QDBusConnectionInterface::AllowReplacement);
    const bool ownsFreedesktop = registration.isValid()
        && registration.value() == QDBusConnectionInterface::ServiceRegistered;
    const auto exportFlags = QDBusConnection::ExportAllSignals | QDBusConnection::ExportAllProperties
        | QDBusConnection::ExportScriptableSlots;
    if (!bus.registerObject(QStringLiteral("/org/aero7/Notifications"), &server, exportFlags)) return 70;
    if (ownsFreedesktop
        && !bus.registerObject(QStringLiteral("/org/freedesktop/Notifications"), &server, exportFlags)) return 70;
    server.setOwnsFreedesktop(ownsFreedesktop);
    if (!ownsFreedesktop) {
        qWarning("Aero7 is using Plasma's hidden notification server as a compatibility backend");
        server.enableCompatibilityBridge();
    }

    QQmlEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("notificationServer"), &server);
    std::unique_ptr<QQmlComponent> compatibilityComponent;
    std::unique_ptr<QObject> compatibilityBridge;
    if (!ownsFreedesktop) {
        compatibilityComponent = std::make_unique<QQmlComponent>(
            &engine, QUrl(QStringLiteral("qrc:/aero7/notifications/CompatibilityBridge.qml")));
        if (compatibilityComponent->isError()) {
            qCritical().noquote() << compatibilityComponent->errorString();
            return 70;
        }
        compatibilityBridge.reset(compatibilityComponent->create(engine.rootContext()));
        if (!compatibilityBridge) {
            qCritical().noquote() << compatibilityComponent->errorString();
            return 70;
        }
    }

    const auto closeUpstream = [&bus](uint id) {
        auto message = QDBusMessage::createMethodCall(
            QStringLiteral("org.freedesktop.Notifications"),
            QStringLiteral("/org/freedesktop/Notifications"),
            QStringLiteral("org.freedesktop.Notifications"),
            QStringLiteral("CloseNotification"));
        message.setArguments({id});
        bus.call(message, QDBus::NoBlock);
    };
    if (!ownsFreedesktop) {
        QObject::connect(&server, &NotificationServer::compatibilityCloseRequested,
                         &application, closeUpstream);
        QObject::connect(&server, &NotificationServer::compatibilityExpireRequested,
                         &application, closeUpstream);
    }
    auto *toast = createWindow(engine, QUrl(QStringLiteral("qrc:/aero7/notifications/ToastView.qml")));
    auto *history = createWindow(engine, QUrl(QStringLiteral("qrc:/aero7/notifications/HistoryView.qml")));
    if (!toast || !history) return 70;
    configureLayer(toast, QGuiApplication::primaryScreen(), 54);
    configureLayer(history, QGuiApplication::primaryScreen(), 54);

    QObject::connect(&server, &NotificationServer::historyScreenChanged, history, [&server, history]() {
        QScreen *target = nullptr;
        for (auto *screen : QGuiApplication::screens()) {
            if (screen->name() == server.historyScreen()) { target = screen; break; }
        }
        if (!target) target = QGuiApplication::primaryScreen();
        configureLayer(history, target, 54);
    });
    QObject::connect(qApp, &QGuiApplication::primaryScreenChanged, toast, [toast](QScreen *screen) {
        configureLayer(toast, screen, 54);
    });
    return application.exec();
}
