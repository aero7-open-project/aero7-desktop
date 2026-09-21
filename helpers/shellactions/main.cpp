#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QScopeGuard>
#include <QTemporaryFile>
#include <QTextStream>
#include <cerrno>
#include <fcntl.h>
#include <systemd/sd-journal.h>
#include <syslog.h>
#include <unistd.h>

namespace {

int journalStream(int priority)
{
    const int stream = sd_journal_stream_fd("aero7-shell-action", priority, 0);
    if (stream < 0 || stream > STDERR_FILENO)
        return stream;
    // Keep it clear of standard descriptors even if the caller closed one.
    const int relocated = fcntl(stream, F_DUPFD_CLOEXEC, STDERR_FILENO + 1);
    close(stream);
    return relocated;
}

bool launch(const QString &program, const QStringList &arguments = {})
{
    QProcess child;
    child.setProgram(program);
    child.setArguments(arguments);
    child.setStandardInputFile(QProcess::nullDevice());

    // Plasma's executable engine closes its pipes when this helper exits.
    // A detached GUI must not inherit them: its first later diagnostic can
    // otherwise terminate it with SIGPIPE. Keep diagnostics in the journal.
    const int output = journalStream(LOG_INFO);
    const int error = journalStream(LOG_WARNING);
    const auto closeStreams = qScopeGuard([output, error] {
        if (output >= 0)
            close(output);
        if (error >= 0)
            close(error);
    });
    if (output >= 0 && error >= 0) {
        child.setChildProcessModifier([output, error, &child] {
            // Only async-signal-safe work is permitted between fork and exec.
            if (dup2(output, STDOUT_FILENO) < 0 || dup2(error, STDERR_FILENO) < 0)
                child.failChildProcessModifier("journal stream redirection", errno);
        });
    } else {
        // A missing journal must not make applications depend on dead pipes.
        QTextStream(stderr) << "Aero7 launcher: journal unavailable; detached application output is discarded\n";
        child.setStandardOutputFile(QProcess::nullDevice());
        child.setStandardErrorFile(QProcess::nullDevice());
    }
    const bool started = child.startDetached();
    if (!started)
        QTextStream(stderr) << "Aero7 launcher: " << child.errorString() << '\n';
    return started;
}

bool evaluatePlasmaScript(const QString &script)
{
    QDBusInterface shell(QStringLiteral("org.kde.plasmashell"),
                         QStringLiteral("/PlasmaShell"),
                         QStringLiteral("org.kde.PlasmaShell"),
                         QDBusConnection::sessionBus());
    if (!shell.isValid())
        return false;
    const QDBusReply<QString> reply = shell.call(QStringLiteral("evaluateScript"), script);
    return reply.isValid();
}

bool toggleTaskbarLock()
{
    return evaluatePlasmaScript(QStringLiteral(
        "for (var p of panels()) {"
        " if (p.type !== 'io.gitgud.wackyideas.panel') continue;"
        " p.locked = !p.locked;"
        "}"));
}

bool toggleShowDesktop()
{
    QDBusInterface kwin(QStringLiteral("org.kde.KWin"),
                        QStringLiteral("/KWin"),
                        QStringLiteral("org.kde.KWin"),
                        QDBusConnection::sessionBus());
    if (!kwin.isValid())
        return false;
    const bool showing = kwin.property("showingDesktop").toBool();
    return kwin.call(QStringLiteral("showDesktop"), !showing).type()
        != QDBusMessage::ErrorMessage;
}

QString arrangeScript(const QString &mode)
{
    return QStringLiteral(R"JS(
const mode = "%1";
const currentDesktopId = workspace.currentDesktop.id;
const candidates = workspace.stackingOrder.filter(function (window) {
    const onThisDesktop = window.onAllDesktops || !window.desktops
        || window.desktops.length === 0
        || window.desktops.some(function (desktop) {
            return desktop.id === currentDesktopId;
        });
    return window.normalWindow && !window.minimized && onThisDesktop
        && window.moveable && window.resizeable;
});
if (candidates.length > 0) {
    const area = workspace.clientArea(KWin.MaximizeArea, candidates[0]);
    const gap = 8;
    if (mode === "cascade") {
        const offset = 28;
        const width = Math.max(320, area.width - offset * Math.min(candidates.length, 8));
        const height = Math.max(220, area.height - offset * Math.min(candidates.length, 8));
        candidates.forEach(function (window, index) {
            window.setMaximize(false, false);
            const step = index % Math.max(1, Math.min(candidates.length, 8));
            window.frameGeometry = {x: area.x + step * offset,
                                    y: area.y + step * offset,
                                    width: width, height: height};
        });
    } else {
        candidates.forEach(function (window, index) {
            window.setMaximize(false, false);
            if (mode === "stacked") {
                const height = Math.floor((area.height - gap * (candidates.length - 1))
                                          / candidates.length);
                window.frameGeometry = {x: area.x,
                                        y: area.y + index * (height + gap),
                                        width: area.width, height: height};
            } else {
                const width = Math.floor((area.width - gap * (candidates.length - 1))
                                         / candidates.length);
                window.frameGeometry = {x: area.x + index * (width + gap),
                                        y: area.y,
                                        width: width, height: area.height};
            }
        });
    }
}
)JS").arg(mode);
}

bool arrangeWindows(const QString &mode)
{
    QTemporaryFile script;
    script.setFileTemplate(QDir(QDir::tempPath()).filePath(
        QCoreApplication::applicationName() + QStringLiteral("-XXXXXX.js")));
    if (!script.open())
        return false;
    script.setAutoRemove(false);
    const QByteArray contents = arrangeScript(mode).toUtf8();
    if (script.write(contents) != contents.size())
        return false;
    const QString path = script.fileName();
    script.close();

    QDBusInterface scripting(QStringLiteral("org.kde.KWin"),
                             QStringLiteral("/Scripting"),
                             QStringLiteral("org.kde.kwin.Scripting"),
                             QDBusConnection::sessionBus());
    const QString plugin = QStringLiteral("aero7-arrange-") + mode;
    scripting.call(QStringLiteral("unloadScript"), plugin);
    const QDBusReply<int> loaded = scripting.call(QStringLiteral("loadScript"), path, plugin);
    if (!loaded.isValid() || loaded.value() < 0) {
        QFile::remove(path);
        return false;
    }
    QDBusInterface scriptObject(
        QStringLiteral("org.kde.KWin"),
        QStringLiteral("/Scripting/Script%1").arg(loaded.value()),
        QStringLiteral("org.kde.kwin.Script"),
        QDBusConnection::sessionBus());
    const QDBusMessage started = scriptObject.call(QStringLiteral("run"));
    scripting.call(QStringLiteral("unloadScript"), plugin);
    QFile::remove(path);
    return started.type() != QDBusMessage::ErrorMessage;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("aero7-shell-action"));
    const QStringList args = app.arguments();
    if (args.size() != 2) {
        QTextStream(stderr) << "Usage: aero7-shell-action ACTION\n";
        return 2;
    }

    const QString action = args.at(1);
    bool ok = false;
    if (action == QLatin1String("explorer"))
        ok = launch(QStringLiteral("aero7-dolphin"));
    else if (action == QLatin1String("task-manager"))
        ok = launch(QStringLiteral("tux-manager"));
    else if (action == QLatin1String("taskbar-properties"))
        ok = launch(QStringLiteral("control"), {QStringLiteral("--page"),
                    QStringLiteral("taskbar-start-menu")});
    else if (action == QLatin1String("start-properties"))
        ok = launch(QStringLiteral("control"), {QStringLiteral("--page"),
                    QStringLiteral("taskbar-start-menu"), QStringLiteral("--tab"),
                    QStringLiteral("start-menu")});
    else if (action == QLatin1String("show-desktop"))
        ok = toggleShowDesktop();
    else if (action == QLatin1String("toggle-taskbar-lock"))
        ok = toggleTaskbarLock();
    else if (action == QLatin1String("cascade"))
        ok = arrangeWindows(QStringLiteral("cascade"));
    else if (action == QLatin1String("stacked"))
        ok = arrangeWindows(QStringLiteral("stacked"));
    else if (action == QLatin1String("side-by-side"))
        ok = arrangeWindows(QStringLiteral("side-by-side"));
    else {
        QTextStream(stderr) << "Unknown Aero7 shell action: " << action << '\n';
        return 2;
    }
    return ok ? 0 : 1;
}
