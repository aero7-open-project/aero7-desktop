#include "../CaptureNotificationGate.h"
#include <QCoreApplication>
#include <QDebug>
#include <QEventLoop>

static void wait(int ms)
{
    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec();
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QObject owner;
    int sent = 0;
    int probes = 0;
    CaptureNotificationGate::deliver(&owner, [&] { ++sent; },
        [&](QObject *, auto reply) { ++probes; reply(false); });
    wait(30);
    if (sent != 1 || probes != 1) return 1;

    sent = probes = 0;
    CaptureNotificationGate::deliver(&owner, [&] { ++sent; },
        [&](QObject *, auto reply) { reply(++probes < 3); });
    wait(30);
    if (sent != 0) return 2;
    wait(180);
    if (sent != 1 || probes != 3) return 3;

    // Persistent user DND is not overridden, and cannot retain the result forever.
    sent = 0;
    CaptureNotificationGate::deliver(&owner, [&] { ++sent; },
        [](QObject *, auto reply) { reply(true); }, 60);
    wait(150);
    if (sent != 1) return 4;

    // A missing reply or a late reply must still deliver exactly once.
    sent = 0;
    CaptureNotificationGate::Reply lateReply;
    CaptureNotificationGate::deliver(&owner, [&] { ++sent; },
        [&](QObject *, auto reply) { lateReply = std::move(reply); }, 30);
    wait(80);
    if (sent != 1 || !lateReply) return 5;
    lateReply(false);
    wait(30);
    if (sent != 1) return 6;

    // Owner destruction cancels pending delivery and its timers/callbacks.
    sent = 0;
    auto *cancelled = new QObject;
    CaptureNotificationGate::deliver(cancelled, [&] { ++sent; },
        [&](QObject *, auto reply) { lateReply = std::move(reply); }, 60);
    wait(10);
    delete cancelled;
    lateReply(false);
    wait(90);
    if (sent != 0) return 7;
    qInfo("Capture notification gate: immediate, transient, persistent, timeout, late reply and owner cancellation passed");
    return 0;
}
