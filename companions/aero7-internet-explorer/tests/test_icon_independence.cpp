#include "IconResources.h"

#include <QApplication>
#include <QHash>
#include <QIcon>
#include <QImage>
#include <QStringList>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    const QStringList resources = {
        QStringLiteral(":/aero7/icons/app/aero7-internet-explorer.png"),
        QStringLiteral(":/aero7/icons/actions/inprivate.png"),
        QStringLiteral(":/aero7/icons/actions/settings.png"),
        QStringLiteral(":/aero7/icons/actions/shortcut.png"),
        QStringLiteral(":/aero7/icons/status/warning.png"),
    };
    QHash<QString, QImage> baseline;
    QIcon::setThemeName(QStringLiteral("Aero7-test-theme"));
    for (const QString &resource : resources) {
        const QImage image = aero7OwnedIcon(resource).pixmap(32, 32).toImage();
        if (image.isNull()) return 1;
        baseline.insert(resource, image);
    }
    for (const QString &theme : {QStringLiteral("breeze"), QStringLiteral("breeze-dark"),
                                 QStringLiteral("missing-aero7-test-theme")}) {
        QIcon::setThemeName(theme);
        for (const QString &resource : resources)
            if (aero7OwnedIcon(resource).pixmap(32, 32).toImage() != baseline.value(resource)) return 2;
    }
    return aero7OwnedIcon(QStringLiteral(":/missing/aero7-icon.svg")).isNull() ? 3 : 0;
}
