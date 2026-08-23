// SPDX-License-Identifier: MIT
#include "TaskbarController.h"

#include <LayerShellQt/Window>

#include <QCommandLineParser>
#include <QDBusConnection>
#include <QGuiApplication>
#include <QHash>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QScreen>
#include <QTimer>

class TaskbarViews final : public QObject
{
public:
    TaskbarViews(QQmlEngine &engine, TaskbarController &controller)
        : m_engine(engine)
        , m_controller(controller)
        , m_component(&engine, QUrl(QStringLiteral("qrc:/aero7/taskbar/TaskbarView.qml")))
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
            } else {
                ++iterator;
            }
        }
        for (auto *screen : screens) {
            if (m_windows.contains(screen)) {
                continue;
            }
            QVariantMap properties;
            properties.insert(QStringLiteral("targetScreen"), QVariant::fromValue(screen));
            properties.insert(QStringLiteral("screenName"), screen->name());
            auto *object = m_component.createWithInitialProperties(properties, m_engine.rootContext());
            auto *window = qobject_cast<QQuickWindow *>(object);
            if (!window) {
                qCritical().noquote() << m_component.errorString();
                delete object;
                continue;
            }
            window->setScreen(screen);
            auto *layer = LayerShellQt::Window::get(window);
            layer->setScreen(screen);
            layer->setScope(QStringLiteral("aero7-taskbar"));
            LayerShellQt::Window::Anchors anchors;
            anchors.setFlag(LayerShellQt::Window::AnchorBottom);
            anchors.setFlag(LayerShellQt::Window::AnchorLeft);
            anchors.setFlag(LayerShellQt::Window::AnchorRight);
            layer->setAnchors(anchors);
            layer->setExclusiveEdge(LayerShellQt::Window::AnchorBottom);
            layer->setExclusiveZone(48);
            layer->setDesiredSize(QSize(0, 48));
            layer->setLayer(LayerShellQt::Window::LayerTop);
            layer->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityOnDemand);
            layer->setCloseOnDismissed(true);
            window->show();
            m_windows.insert(screen, window);
        }
        m_controller.setScreenCount(m_windows.size());
    }

    QQmlEngine &m_engine;
    TaskbarController &m_controller;
    QQmlComponent m_component;
    QHash<QScreen *, QQuickWindow *> m_windows;
};

int main(int argc, char **argv)
{
    QGuiApplication application(argc, argv);
    application.setApplicationName(QStringLiteral("aero7-taskbar"));
    application.setApplicationDisplayName(QStringLiteral("Aero7 Taskbar"));
    application.setOrganizationName(QStringLiteral("Aero7"));
    application.setDesktopFileName(QStringLiteral("org.aero7.taskbar"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Aero7 synchronized Wayland taskbar"));
    parser.addHelpOption();
    QCommandLineOption dumpState(QStringLiteral("dump-state"), QStringLiteral("Print model state and exit."));
    parser.addOption(dumpState);
    parser.process(application);

    auto bus = QDBusConnection::sessionBus();
    if (!bus.registerService(QStringLiteral("org.aero7.Taskbar"))) {
        qCritical() << "Another Aero7 taskbar instance is already active";
        return 73;
    }

    TaskbarController controller;
    bus.registerObject(QStringLiteral("/Taskbar"), &controller,
                       QDBusConnection::ExportAllSignals | QDBusConnection::ExportAllProperties
                           | QDBusConnection::ExportScriptableSlots);

    if (parser.isSet(dumpState)) {
        QTimer::singleShot(250, &application, [&application, &controller]() {
            qInfo().noquote() << controller.dumpState();
            application.quit();
        });
        return application.exec();
    }

    QQmlEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("taskbarController"), &controller);
    TaskbarViews views(engine, controller);
    if (!views.initialize()) {
        return 70;
    }
    return application.exec();
}
