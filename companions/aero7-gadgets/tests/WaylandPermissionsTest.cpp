#include <QCoreApplication>
#include <KService>
#include <cstdio>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    if (argc != 2) return 2;
    const QStringList expected{QStringLiteral("org_kde_plasma_window_management")};
    bool valid = true;
    for (const auto &name : {"org.aero7.GadgetHost", "org.aero7.GadgetGallery", "org.aero7.GadgetHost-autostart"}) {
        KService service(QString::fromLocal8Bit(argv[1]) + QStringLiteral("/data/")
                         + QString::fromLatin1(name) + QStringLiteral(".desktop"));
        // KWin reads this custom key through KService, not the standard desktop
        // entry semicolon-list parser. Test the parsed values it actually sees.
        const auto actual = service.property<QStringList>(QStringLiteral("X-KDE-Wayland-Interfaces"));
        if (actual != expected) {
            fprintf(stderr, "%s permission mismatch: [%s], expected [%s]\n", name,
                    qPrintable(actual.join(QStringLiteral(","))), qPrintable(expected.join(QStringLiteral(","))));
            valid = false;
        }
    }
    return valid ? 0 : 1;
}
