#include <QApplication>
#include <QHash>
#include <QIcon>
#include <QImage>
#include <QStringList>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    const QStringList resources = {
        QStringLiteral(":/aero7/icons/app/aero7-snipping-tool.png"),
        QStringLiteral(":/aero7/icons/status/error.png"),
    };
    QHash<QString, QImage> baseline;
    QIcon::setThemeName(QStringLiteral("Aero7-test-theme"));
    for (const QString &path : resources) {
        const QImage image = QIcon(path).pixmap(32, 32).toImage();
        if (image.isNull()) return 1;
        baseline.insert(path, image);
    }
    for (const QString &theme : {QStringLiteral("breeze"),
                                 QStringLiteral("breeze-dark"),
                                 QStringLiteral("missing-aero7-test-theme")}) {
        QIcon::setThemeName(theme);
        for (const QString &path : resources)
            if (QIcon(path).pixmap(32, 32).toImage() != baseline.value(path)) return 2;
    }
    return 0;
}
