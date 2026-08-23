// SPDX-License-Identifier: MIT
#include "TrayController.h"

#include <LayerShellQt/Window>

#include <QDBusConnection>
#include <QGuiApplication>
#include <QHash>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QScreen>

class TrayViews final : public QObject
{
public:
    TrayViews(QQmlEngine &engine, TrayController &controller)
        : m_engine(engine), m_controller(controller),
          m_component(&engine, QUrl(QStringLiteral("qrc:/aero7/tray/TrayView.qml")))
    {
        connect(qApp, &QGuiApplication::screenAdded, this, [this](QScreen *) { synchronize(); });
        connect(qApp, &QGuiApplication::screenRemoved, this, [this](QScreen *) { synchronize(); });
    }

    bool initialize()
    {
        if (m_component.isError()) {
            qCritical().noquote() << m_component.errorString();
            return false;
        }
        synchronize();
        return !m_windows.isEmpty();
    }

private:
    void synchronize()
    {
        const auto screens = QGuiApplication::screens();
        for (auto iterator = m_windows.begin(); iterator != m_windows.end();) {
            if (!screens.contains(iterator.key())) {
                iterator.value()->close();
                iterator.value()->deleteLater();
                iterator = m_windows.erase(iterator);
            } else ++iterator;
        }
        for (auto *screen : screens) {
            if (m_windows.contains(screen)) continue;
            QVariantMap properties{{QStringLiteral("targetScreen"), QVariant::fromValue(screen)},
                                   {QStringLiteral("screenName"), screen->name()}};
            auto *window = qobject_cast<QQuickWindow *>(
                m_component.createWithInitialProperties(properties, m_engine.rootContext()));
            if (!window) continue;
            window->setScreen(screen);
            auto *layer = LayerShellQt::Window::get(window);
            LayerShellQt::Window::Anchors anchors;
            anchors.setFlag(LayerShellQt::Window::AnchorBottom);
            anchors.setFlag(LayerShellQt::Window::AnchorRight);
            layer->setAnchors(anchors);
            layer->setScreen(screen);
            layer->setScope(QStringLiteral("aero7-tray"));
            layer->setExclusiveZone(0);
            layer->setMargins(QMargins(0, 0, 12, 0));
            layer->setDesiredSize(QSize(320, 48));
            layer->setLayer(LayerShellQt::Window::LayerOverlay);
            layer->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityOnDemand);
            window->show();
            m_windows.insert(screen, window);
        }
        m_controller.setScreenCount(m_windows.size());
    }
    QQmlEngine &m_engine;
    TrayController &m_controller;
    QQmlComponent m_component;
    QHash<QScreen *, QQuickWindow *> m_windows;
};

int main(int argc, char **argv)
{
    QGuiApplication application(argc, argv);
    application.setApplicationName(QStringLiteral("aero7-tray"));
    application.setApplicationDisplayName(QStringLiteral("Aero7 Notification Area"));
    application.setDesktopFileName(QStringLiteral("org.aero7.tray"));
    application.setOrganizationName(QStringLiteral("Aero7"));

    auto bus = QDBusConnection::sessionBus();
    if (!bus.registerService(QStringLiteral("org.aero7.Tray"))) return 73;
    TrayController controller;
    bus.registerObject(QStringLiteral("/Tray"), &controller,
                       QDBusConnection::ExportAllSignals | QDBusConnection::ExportAllProperties
                           | QDBusConnection::ExportScriptableSlots);
    QQmlEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("trayController"), &controller);
    TrayViews views(engine, controller);
    if (!views.initialize()) return 70;
    return application.exec();
}
