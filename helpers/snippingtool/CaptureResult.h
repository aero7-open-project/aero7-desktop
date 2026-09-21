#pragma once

#include <QProcess>

enum class CaptureResult { Cancelled, Failed, ImageReady };

inline CaptureResult captureResult(int exitCode, QProcess::ExitStatus exitStatus,
                                   bool outputExists, qint64 outputSize)
{
    // A crash/nonzero exit is not a user cancellation, even without a file.
    if (exitStatus != QProcess::NormalExit || exitCode != 0)
        return CaptureResult::Failed;
    // Spectacle's background selector exits cleanly without output on Escape.
    if (!outputExists)
        return CaptureResult::Cancelled;
    return outputSize > 0 ? CaptureResult::ImageReady : CaptureResult::Failed;
}
