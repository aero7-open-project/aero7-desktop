#include <KGlobalAccel>
#include <KSystemClipboard>
#include <KNotification>
#include "CaptureResult.h"
#include "CaptureClipboardData.h"
#include "CaptureNotificationGate.h"
#include "PendingCaptureWindow.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QImage>
#include <QIcon>
#include <QKeySequence>
#include <QMimeData>
#include <QProcess>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <QUuid>

static QIcon ownedIcon(const QString &resource)
{
    const QIcon icon(resource);
    if (!icon.isNull())
        return icon;
    qWarning().noquote() << "[Aero7 Icons] Missing resource:" << resource;
    QPixmap fallback(32, 32);
    fallback.fill(Qt::transparent);
    return QIcon(fallback);
}

class SnippingTool final : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.aero7.SnippingTool")

public:
    explicit SnippingTool(QObject *parent = nullptr)
        : QObject(parent)
    {
        KSystemClipboard::instance();
        m_pending.cancelled = [this] { abortPreparation(true); };
        m_pending.timedOut = [this] { abortPreparation(false); };
        m_cancelShortcut.setObjectName(QStringLiteral("cancelActiveCapture"));
        m_cancelShortcut.setText(QStringLiteral("Cancel active screenshot"));
        // KGlobalAccel keeps registrations when a client crashes. Reattach
        // only our internal capture action with no key, then remove it. A
        // blanket component cleanup could discard unrelated user shortcuts.
        KGlobalAccel::self()->setShortcut(&m_cancelShortcut, {},
                                          KGlobalAccel::NoAutoloading);
        releaseCancelShortcut();
        connect(&m_cancelShortcut, &QAction::triggered, this, [this] {
            if (m_process.state() != QProcess::NotRunning) {
                m_pending.disarm();
                abortPreparation(true);
            }
        });
        connect(&m_process, &QProcess::started, this, [this] {
            // Escape can arrive while QProcess is still starting.
            if (m_abort != PreparationAbort::None) m_process.terminate();
        });
        m_shortcut.setObjectName(QStringLiteral("rectangularSnip"));
        m_shortcut.setText(QStringLiteral("Rectangular Snip"));
        const QKeySequence sequence(QStringLiteral("Meta+Shift+S"));
        KGlobalAccel::stealShortcutSystemwide(sequence);
        KGlobalAccel::self()->setDefaultShortcut(&m_shortcut, {sequence},
                                                 KGlobalAccel::NoAutoloading);
        KGlobalAccel::self()->setShortcut(&m_shortcut, {sequence},
                                          KGlobalAccel::NoAutoloading);
        connect(&m_shortcut, &QAction::triggered, this, &SnippingTool::capture);
        connect(&m_process, &QProcess::finished, this, &SnippingTool::captureFinished);
        connect(&m_process, &QProcess::errorOccurred, this,
                [this](QProcess::ProcessError error) {
            if (error == QProcess::FailedToStart) {
                m_pending.disarm();
                releaseCancelShortcut();
                if (m_abort != PreparationAbort::Cancelled)
                    captureFailed(QStringLiteral("Spectacle could not be started."));
                startQueuedCapture();
            }
        });
    }

public Q_SLOTS:
    void capture()
    {
        if (m_process.state() != QProcess::NotRunning) {
            m_captureQueued = true;
            return;
        }

        const QString pictures = QStandardPaths::writableLocation(
            QStandardPaths::PicturesLocation);
        if (pictures.isEmpty()) {
            captureFailed(QStringLiteral("The Pictures folder is not configured."));
            return;
        }
        QDir screenshots(QDir(pictures).filePath(QStringLiteral("Screenshots")));
        if (!screenshots.exists() && !QDir().mkpath(screenshots.path())) {
            captureFailed(QStringLiteral("The Screenshots folder could not be created."));
            return;
        }
        const QFileInfo folderInfo(screenshots.path());
        if (!folderInfo.isDir() || !folderInfo.isWritable()) {
            captureFailed(QStringLiteral("The Screenshots folder is not writable."));
            return;
        }

        const QString stamp = QDateTime::currentDateTime().toString(
            QStringLiteral("yyyy-MM-dd hh.mm.ss.zzz"));
        m_outputPath = screenshots.filePath(
            QStringLiteral("Screenshot %1-%2.png")
                .arg(stamp, QUuid::createUuid().toString(QUuid::Id128).left(6)));
        m_process.setProgram(qEnvironmentVariable(
            "AERO7_SPECTACLE_EXECUTABLE", QStringLiteral("spectacle")));
        m_process.setArguments({QStringLiteral("--desktopfile"),
                                QStringLiteral("org.kde.spectacle"),
                                QStringLiteral("--new-instance"),
                                QStringLiteral("--region"),
                                QStringLiteral("--release-capture"),
                                QStringLiteral("--background"),
                                QStringLiteral("--nonotify"),
                                QStringLiteral("--output"), m_outputPath});
        ++m_generation;
        m_abort = PreparationAbort::None;
        // The focus transition to Spectacle has a small interval in which
        // neither surface reliably receives Escape. Reserve it only for the
        // duration of this capture. Never steal an existing user shortcut.
        const QKeySequence escape(Qt::Key_Escape);
        if (KGlobalAccel::globalShortcutsByKey(escape).isEmpty())
            KGlobalAccel::self()->setShortcut(&m_cancelShortcut, {escape},
                                              KGlobalAccel::NoAutoloading);
        if (!KGlobalAccel::self()->shortcut(&m_cancelShortcut).contains(escape))
            qWarning() << "Aero7 Snipping Tool: capture Escape unavailable; using selector focus.";
        m_pending.arm();
        m_process.start();
    }

private Q_SLOTS:
    void captureFinished(int exitCode, QProcess::ExitStatus exitStatus)
    {
        m_pending.disarm();
        releaseCancelShortcut();
        if (m_abort != PreparationAbort::None) {
            // This unpredictable per-attempt path belongs to our cancelled
            // capture, not a user-selected existing file. Never copy it.
            if (QFileInfo::exists(m_outputPath) && !QFile::remove(m_outputPath))
                captureFailed(QStringLiteral("The cancelled screenshot could not be removed."));
            else if (m_abort == PreparationAbort::TimedOut)
                captureFailed(QStringLiteral("Screenshot selection did not become ready in time."));
            startQueuedCapture();
            return;
        }
        const QFileInfo output(m_outputPath);
        const auto result = captureResult(exitCode, exitStatus, output.exists(), output.size());
        if (result == CaptureResult::Cancelled) {
            // A clean selector cancellation leaves the clipboard unchanged.
        } else if (result == CaptureResult::Failed) {
            captureFailed(QString::fromUtf8(m_process.readAllStandardError()).trimmed());
        } else {
            const QImage image(m_outputPath);
            auto mimeData = captureClipboardData(image);
            if (!mimeData) {
                captureFailed(QStringLiteral("The captured PNG could not be prepared for the clipboard."));
            } else {
                KSystemClipboard::instance()->setMimeData(
                    mimeData.release(), QClipboard::Clipboard);
                screenshotSaved(image);
            }
        }

        startQueuedCapture();
    }

private:
    enum class PreparationAbort { None, Cancelled, TimedOut };

    void releaseCancelShortcut()
    {
        KGlobalAccel::self()->removeAllShortcuts(&m_cancelShortcut);
    }

    void abortPreparation(bool userCancelled)
    {
        m_abort = userCancelled ? PreparationAbort::Cancelled : PreparationAbort::TimedOut;
        m_captureQueued = false;
        releaseCancelShortcut();
        m_process.terminate();
        const auto generation = m_generation;
        // Bound shutdown of an unresponsive backend. A delayed timer must
        // never kill a subsequent capture after the cancelled one exits.
        QTimer::singleShot(500, this, [this, generation] {
            if (m_generation == generation && m_abort != PreparationAbort::None
                && m_process.state() != QProcess::NotRunning)
                m_process.kill();
        });
    }

    void startQueuedCapture()
    {
        if (m_captureQueued) {
            m_captureQueued = false;
            QTimer::singleShot(0, this, &SnippingTool::capture);
        }
    }

    void screenshotSaved(const QImage &image)
    {
        auto *notification = new KNotification(
            QStringLiteral("screenshotSaved"), KNotification::CloseOnTimeout, this);
        notification->setComponentName(QStringLiteral("aero7-snipping-tool"));
        notification->setTitle(QStringLiteral("Screenshot saved"));
        notification->setText(QStringLiteral(
            "The screenshot was saved to Screenshots and copied to the clipboard."));
        notification->setIconName(QStringLiteral("aero7-snipping-tool"));
        notification->setPixmap(QPixmap::fromImage(image.scaled(
            320, 200, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
        notification->setUrls({QUrl::fromLocalFile(m_outputPath)});
        auto *open = notification->addDefaultAction(QStringLiteral("Open"));
        const QString path = m_outputPath;
        connect(open, &KNotificationAction::activated, notification, [path]() {
            QDesktopServices::openUrl(QUrl::fromLocalFile(path));
        });
        CaptureNotificationGate::deliver(notification, [notification] { notification->sendEvent(); });
    }

    void captureFailed(QString detail)
    {
        detail = detail.trimmed();
        if (detail.isEmpty())
            detail = QStringLiteral("The screenshot could not be captured.");
        qWarning().noquote() << "Aero7 Snipping Tool:" << detail;
        auto *notification = new KNotification(
            QStringLiteral("screenshotFailed"), KNotification::CloseOnTimeout, this);
        notification->setComponentName(QStringLiteral("aero7-snipping-tool"));
        notification->setTitle(QStringLiteral("Snipping Tool"));
        notification->setText(QStringLiteral("The screenshot could not be captured."));
        notification->setIconName(QStringLiteral("aero7-snipping-tool"));
        notification->setPixmap(ownedIcon(QStringLiteral(":/aero7/icons/status/error.png"))
                                    .pixmap(48, 48));
        auto *retry = notification->addAction(QStringLiteral("Try Again"));
        connect(retry, &KNotificationAction::activated, this, &SnippingTool::capture);
        CaptureNotificationGate::deliver(notification, [notification] { notification->sendEvent(); });
    }

    QAction m_shortcut;
    QAction m_cancelShortcut;
    QProcess m_process;
    QString m_outputPath;
    bool m_captureQueued = false;
    PendingCaptureWindow m_pending;
    PreparationAbort m_abort = PreparationAbort::None;
    quint64 m_generation = 0;
};

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("aero7-snipping-tool"));
    app.setOrganizationName(QStringLiteral("Aero7"));
    // Qt registers this ID with the desktop portal. Set it before creating
    // windows/clipboard objects; an executable name is not a desktop-file ID.
    app.setDesktopFileName(QStringLiteral("org.aero7.snippingtool"));
    app.setWindowIcon(ownedIcon(QStringLiteral(":/aero7/icons/app/aero7-snipping-tool.png")));
    app.setQuitOnLastWindowClosed(false);

    if (app.arguments().contains(QStringLiteral("--check-backend"))) {
        QProcess help;
        help.start(qEnvironmentVariable("AERO7_SPECTACLE_EXECUTABLE",
                                        QStringLiteral("spectacle")),
                   {QStringLiteral("--help")});
        if (!help.waitForStarted(3000) || !help.waitForFinished(5000)
            || help.exitCode() != 0) {
            qCritical("Spectacle is not available.");
            return 1;
        }
        const QByteArray output = help.readAllStandardOutput();
        const QList<QByteArray> required = {"--desktopfile", "--region", "--release-capture",
                                             "--background", "--nonotify", "--output", "--new-instance"};
        for (const QByteArray &option : required) {
            if (!output.contains(option)) {
                qCritical().noquote() << "Spectacle does not support" << option;
                return 1;
            }
        }
        qInfo("Spectacle rectangular background capture interface: OK");
        return 0;
    }

    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.registerService(QStringLiteral("org.aero7.SnippingTool"))) {
        if (app.arguments().contains(QStringLiteral("--capture"))) {
            QDBusInterface running(QStringLiteral("org.aero7.SnippingTool"),
                                   QStringLiteral("/SnippingTool"),
                                   QStringLiteral("org.aero7.SnippingTool"), bus);
            return running.call(QStringLiteral("capture")).type()
                == QDBusMessage::ErrorMessage ? 1 : 0;
        }
        return 0;
    }

    SnippingTool tool;
    bus.registerObject(QStringLiteral("/SnippingTool"), &tool,
                       QDBusConnection::ExportAllSlots);
    if (app.arguments().contains(QStringLiteral("--capture")))
        QTimer::singleShot(0, &tool, &SnippingTool::capture);
    return app.exec();
}

#include "main.moc"
