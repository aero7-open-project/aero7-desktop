#include "../PendingCaptureWindow.h"
#include <QApplication>
#include <QDebug>
#include <QEventLoop>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    PendingCaptureWindow window;
    int cancellations = 0, timeouts = 0;
    window.cancelled = [&] { ++cancellations; };
    window.timedOut = [&] { ++timeouts; };
    auto key = [&](int code) {
        QKeyEvent event(QEvent::KeyPress, code, Qt::NoModifier);
        QApplication::sendEvent(&window, &event);
    };
    auto focus = [&](QEvent::Type type) {
        QEvent event(type);
        QApplication::sendEvent(&window, &event);
    };
    window.arm();
    key(Qt::Key_A);
    if (!window.isArmed() || cancellations) return 1;
    key(Qt::Key_Escape); // cancellation before activation arrives
    focus(QEvent::WindowActivate); // stale activation must not re-arm
    key(Qt::Key_Escape);
    if (window.isArmed() || window.isVisible() || cancellations != 1) return 2;
    window.arm();
    focus(QEvent::WindowActivate);
    focus(QEvent::WindowDeactivate); // actual selector owns focus now
    key(Qt::Key_Escape);
    if (window.isArmed() || window.isVisible() || cancellations != 1) return 3;
    window.arm(0);
    QEventLoop loop;
    QTimer::singleShot(20, &loop, &QEventLoop::quit);
    loop.exec();
    if (window.isArmed() || window.isVisible() || timeouts != 1) return 4;
    window.arm(0);
    window.disarm(); // completed/failed process clears the pending deadline
    QTimer::singleShot(20, &loop, &QEventLoop::quit);
    loop.exec();
    if (timeouts != 1 || cancellations != 1) return 5;
    qInfo() << "Pending capture cancellation, focus handoff and deadline cases passed";
    return 0;
}
