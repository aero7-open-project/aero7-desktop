#include "gadgetdrop.h"

#include <QProcess>
#include <QStandardPaths>

bool GadgetDrop::add(const QString &id, const QString &screenName, int x, int y) const
{
    if (!id.startsWith(QStringLiteral("org.aero7.gadgets.")) || screenName.isEmpty()) {
        return false;
    }
    const QString host = QStandardPaths::findExecutable(QStringLiteral("aero7-gadget-host"));
    if (host.isEmpty()) {
        return false;
    }
    return QProcess::startDetached(host, {
        QStringLiteral("--drop-id"), id,
        QStringLiteral("--drop-screen"), screenName,
        QStringLiteral("--drop-x"), QString::number(x),
        QStringLiteral("--drop-y"), QString::number(y),
    });
}
