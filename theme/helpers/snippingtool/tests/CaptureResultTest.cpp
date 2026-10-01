#include "../CaptureResult.h"
#include <QCoreApplication>
#include <QDebug>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    struct Case {
        const char *name;
        int code;
        QProcess::ExitStatus status;
        bool exists;
        qint64 size;
        CaptureResult expected;
    };
    const Case cases[] = {
        {"Escape without output", 0, QProcess::NormalExit, false, 0, CaptureResult::Cancelled},
        {"Successful PNG", 0, QProcess::NormalExit, true, 100, CaptureResult::ImageReady},
        {"Empty output", 0, QProcess::NormalExit, true, 0, CaptureResult::Failed},
        {"Failure without output", 1, QProcess::NormalExit, false, 0, CaptureResult::Failed},
        {"Failure with partial output", 1, QProcess::NormalExit, true, 100, CaptureResult::Failed},
        {"Crash without output", 9, QProcess::CrashExit, false, 0, CaptureResult::Failed},
        {"Crash with partial output", 9, QProcess::CrashExit, true, 100, CaptureResult::Failed},
        {"Abnormal zero status", 0, QProcess::CrashExit, false, 0, CaptureResult::Failed},
    };
    for (const auto &test : cases) {
        if (captureResult(test.code, test.status, test.exists, test.size) != test.expected) {
            qCritical() << test.name;
            return 1;
        }
    }
    QProcess failure;
    failure.start(QStringLiteral("/bin/false"));
    if (!failure.waitForFinished(5000)
        || captureResult(failure.exitCode(), failure.exitStatus(), false, 0) != CaptureResult::Failed)
        return 2;
    return 0;
}
