#pragma once
#include <QDebug>
#include <QIcon>
#include <QString>

inline QIcon aero7OwnedIcon(const QString &path)
{
    const QIcon icon(path);
    if (!icon.isNull()) return icon;
    qWarning().noquote() << "[Aero7 Icons] Missing resource:" << path;
    return QIcon(QStringLiteral(":/aero7/icons/app/aero7-internet-explorer.png"));
}
