// SPDX-License-Identifier: MIT
#include "DesktopController.h"

#include <LayerShellQt/Window>
#include <QDBusConnection>
#include <QGuiApplication>
#include <QHash>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QScreen>

class DesktopViews final : public QObject
{
public:
    DesktopViews(QQmlEngine &engine, DesktopController &controller)
        : m_engine(engine), m_controller(controller),
          m_component(&engine, QUrl(QStringLiteral("qrc:/aero7/desktop/DesktopView.qml")))
    {
        connect(qApp, &QGuiApplication::screenAdded, this, [this](QScreen *) { synchronize(); });
        connect(qApp, &QGuiApplication::screenRemoved, this, [this](QScreen *) { synchronize(); });
        connect(qApp, &QGuiApplication::primaryScreenChanged, this, [this](QScreen *) { rebuild(); });
    }
    bool initialize() { synchronize(); return !m_component.isError() && !m_windows.isEmpty(); }

private:
    void rebuild()
    {
        for (auto *window : std::as_const(m_windows)) { window->close(); window->deleteLater(); }
        m_windows.clear();
        synchronize();
    }
    void synchronize()
    {
        const auto screens = QGuiApplication::screens();
        for (auto iterator = m_windows.begin(); iterator != m_windows.end();) {
            if (!screens.contains(iterator.key())) { iterator.value()->close(); iterator.value()->deleteLater(); iterator = m_windows.erase(iterator); }
            else ++iterator;
        }
        for (auto *screen : screens) {
            if (m_windows.contains(screen)) continue;
            const QVariantMap properties{{QStringLiteral("targetScreen"), QVariant::fromValue(screen)},
                                         {QStringLiteral("screenName"), screen->name()},
                                         {QStringLiteral("primary"), screen == QGuiApplication::primaryScreen()}};
            auto *window = qobject_cast<QQuickWindow *>(m_component.createWithInitialProperties(properties, m_engine.rootContext()));
            if (!window) continue;
            window->setScreen(screen);
            auto *layer = LayerShellQt::Window::get(window);
            LayerShellQt::Window::Anchors anchors;
            anchors.setFlag(LayerShellQt::Window::AnchorTop);
            anchors.setFlag(LayerShellQt::Window::AnchorBottom);
            anchors.setFlag(LayerShellQt::Window::AnchorLeft);
            anchors.setFlag(LayerShellQt::Window::AnchorRight);
            layer->setAnchors(anchors);
            layer->setScreen(screen);
            layer->setScope(QStringLiteral("aero7-desktop"));
            layer->setExclusiveZone(-1);
            layer->setLayer(LayerShellQt::Window::LayerBottom);
            layer->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityOnDemand);
            window->show();
            m_windows.insert(screen, window);
        }
        m_controller.setScreenCount(m_windows.size());
    }
    QQmlEngine &m_engine;
    DesktopController &m_controller;
    QQmlComponent m_component;
    QHash<QScreen *, QQuickWindow *> m_windows;
};

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("aero7-desktop-surface"));
    app.setApplicationDisplayName(QStringLiteral("Aero7 Desktop"));
    app.setDesktopFileName(QStringLiteral("org.aero7.desktop"));
    app.setOrganizationName(QStringLiteral("Aero7"));
    auto bus = QDBusConnection::sessionBus();
    if (!bus.registerService(QStringLiteral("org.aero7.Desktop"))) return 73;
    DesktopController controller;
    bus.registerObject(QStringLiteral("/Desktop"), &controller,
                       QDBusConnection::ExportAllSignals | QDBusConnection::ExportAllProperties
                           | QDBusConnection::ExportScriptableSlots);
    QQmlEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("desktopController"), &controller);
    DesktopViews views(engine, controller);
    if (!views.initialize()) return 70;
    return app.exec();
}
