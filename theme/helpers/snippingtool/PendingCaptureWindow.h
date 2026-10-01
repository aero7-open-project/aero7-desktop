#pragma once

#include <QKeyEvent>
#include <QTimer>
#include <QWidget>
#include <QWindow>
#include <functional>

// Own keyboard focus only while the backend prepares its selector. A normal
// tool surface avoids a taskbar entry or a global Escape registration. Its
// transparent contents add no image pixels; the real selector takes focus
// through the normal window activation path.
class PendingCaptureWindow final : public QWidget
{
public:
    PendingCaptureWindow()
        : QWidget(nullptr, Qt::Tool | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint)
    {
        setAttribute(Qt::WA_TranslucentBackground);
        setWindowTitle(QStringLiteral("Snipping Tool"));
        setFocusPolicy(Qt::StrongFocus);
        m_deadline.setSingleShot(true);
        connect(&m_deadline, &QTimer::timeout, this, [this] {
            if (!m_armed) return;
            disarm();
            if (timedOut) timedOut();
        });
    }

    std::function<void()> cancelled;
    std::function<void()> timedOut;

    void arm(int deadlineMs = 10000)
    {
        m_armed = true;
        m_hadFocus = false;
        showFullScreen();
        windowHandle()->requestActivate();
        m_deadline.start(deadlineMs);
    }

    void disarm()
    {
        m_armed = false;
        m_hadFocus = false;
        m_deadline.stop();
        hide();
    }

    bool isArmed() const { return m_armed; }

protected:
    bool event(QEvent *event) override
    {
        if (m_armed && event->type() == QEvent::WindowActivate) {
            m_hadFocus = true;
        } else if (m_armed && m_hadFocus && event->type() == QEvent::WindowDeactivate) {
            // Never retain an invisible surface over the selection window.
            disarm();
        }
        return QWidget::event(event);
    }

    void keyPressEvent(QKeyEvent *event) override
    {
        if (m_armed && event->key() == Qt::Key_Escape) {
            event->accept();
            disarm();
            if (cancelled) cancelled();
            return;
        }
        QWidget::keyPressEvent(event);
    }

private:
    QTimer m_deadline;
    bool m_armed = false;
    bool m_hadFocus = false;
};
