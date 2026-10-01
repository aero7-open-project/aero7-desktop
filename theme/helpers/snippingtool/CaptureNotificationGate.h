#pragma once

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusVariant>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <functional>
#include <utility>

// The selector process can exit before Plasma clears its fullscreen inhibition.
// Wait briefly for that state to settle, without changing the user's DND state
// or marking ordinary screenshot notifications as critical. A persistent DND
// state or unavailable server still gets normal notification/history delivery.
class CaptureNotificationGate final : public QObject
{
public:
    using Reply = std::function<void(bool inhibited)>;
    using Probe = std::function<void(QObject *context, Reply reply)>;

    static void deliver(QObject *owner, std::function<void()> send,
                        Probe probe = queryInhibition, int deadlineMs = 1000)
    {
        new CaptureNotificationGate(owner, std::move(send), std::move(probe), deadlineMs);
    }

private:
    CaptureNotificationGate(QObject *owner, std::function<void()> send, Probe probe, int deadlineMs)
        : QObject(owner), m_send(std::move(send)), m_probe(std::move(probe))
    {
        m_deadline.setSingleShot(true);
        connect(&m_deadline, &QTimer::timeout, this, [this] { finish(); });
        m_deadline.start(deadlineMs);
        QTimer::singleShot(0, this, [this] { poll(); });
    }

    static void queryInhibition(QObject *context, Reply reply)
    {
        auto message = QDBusMessage::createMethodCall(
            QStringLiteral("org.freedesktop.Notifications"),
            QStringLiteral("/org/freedesktop/Notifications"),
            QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("Get"));
        message.setArguments({QStringLiteral("org.freedesktop.Notifications"),
                              QStringLiteral("Inhibited")});
        auto *watcher = new QDBusPendingCallWatcher(
            QDBusConnection::sessionBus().asyncCall(message, 200), context);
        connect(watcher, &QDBusPendingCallWatcher::finished, context,
                [watcher, reply = std::move(reply)] {
            const QDBusPendingReply<QDBusVariant> result = *watcher;
            const bool inhibited = !result.isError() && result.value().variant().toBool();
            watcher->deleteLater();
            reply(inhibited);
        });
    }

    void poll()
    {
        if (m_done) return;
        QPointer<CaptureNotificationGate> guard(this);
        m_probe(this, [guard](bool inhibited) {
            if (!guard || guard->m_done) return;
            if (!inhibited) {
                guard->finish();
            } else {
                QTimer::singleShot(50, guard, [guard] { if (guard) guard->poll(); });
            }
        });
    }

    void finish()
    {
        if (m_done) return;
        m_done = true;
        m_deadline.stop();
        auto send = std::move(m_send);
        deleteLater();
        send();
    }

    std::function<void()> m_send;
    Probe m_probe;
    QTimer m_deadline;
    bool m_done = false;
};
