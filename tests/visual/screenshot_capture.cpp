// SPDX-License-Identifier: MIT
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusUnixFileDescriptor>
#include <QCommandLineParser>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QJsonDocument>
#include <QThread>
#include <QVariantMap>

#include <cerrno>
#include <unistd.h>

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
    QGuiApplication application(argc, argv);
    application.setApplicationName(QStringLiteral("Aero7 visual test capture"));
    application.setDesktopFileName(QStringLiteral("org.aero7.visualtest"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Authorized Aero7 VM screenshot helper"));
    parser.addHelpOption();
    const QCommandLineOption delayOption({QStringLiteral("d"), QStringLiteral("delay-ms")},
                                         QStringLiteral("Wait before capturing transient shell UI."),
                                         QStringLiteral("milliseconds"), QStringLiteral("0"));
    parser.addOption(delayOption);
    parser.addPositionalArgument(QStringLiteral("output"), QStringLiteral("Output PNG path."),
                                 QStringLiteral("[OUTPUT.png]"));
    parser.process(application);

    if (parser.positionalArguments().size() > 1) {
        parser.showHelp(64);
    }
    bool delayOk = false;
    const int delayMilliseconds = parser.value(delayOption).toInt(&delayOk);
    if (!delayOk || delayMilliseconds < 0 || delayMilliseconds > 30000) {
        qCritical("--delay-ms must be between 0 and 30000");
        return 64;
    }
    const QString outputPath = parser.positionalArguments().size() == 1
        ? parser.positionalArguments().constFirst()
        : QStringLiteral("/tmp/aero7-visual-test.png");

    if (delayMilliseconds > 0) {
        QThread::msleep(static_cast<unsigned long>(delayMilliseconds));
    }

    int descriptors[2];
    if (pipe(descriptors) != 0) {
        qCritical("Could not create screenshot pipe: %s", qPrintable(QString::fromLocal8Bit(strerror(errno))));
        return 1;
    }

    QDBusInterface screenshot(QStringLiteral("org.kde.KWin.ScreenShot2"),
                              QStringLiteral("/org/kde/KWin/ScreenShot2"),
                              QStringLiteral("org.kde.KWin.ScreenShot2"));
    QVariantMap options{{QStringLiteral("include-cursor"), false},
                        {QStringLiteral("native-resolution"), false},
                        {QStringLiteral("hide-caller-windows"), false}};
    QDBusUnixFileDescriptor writeDescriptor(descriptors[1]);
    const QDBusReply<QVariantMap> reply = screenshot.call(QStringLiteral("CaptureActiveScreen"),
                                                          options, QVariant::fromValue(writeDescriptor));
    close(descriptors[1]);
    if (!reply.isValid()) {
        close(descriptors[0]);
        qCritical("Screenshot failed: %s", qPrintable(reply.error().message()));
        return 1;
    }

    const auto metadata = reply.value();
    qInfo().noquote() << QJsonDocument::fromVariant(metadata).toJson(QJsonDocument::Compact);
    const auto width = metadata.value(QStringLiteral("width")).toUInt();
    const auto height = metadata.value(QStringLiteral("height")).toUInt();
    const auto stride = metadata.value(QStringLiteral("stride")).toUInt();
    const auto format = static_cast<QImage::Format>(metadata.value(QStringLiteral("format")).toUInt());
    const qsizetype expected = qsizetype(stride) * height;
    QByteArray pixels(expected, Qt::Uninitialized);
    qsizetype offset = 0;
    while (offset < expected) {
        const auto count = read(descriptors[0], pixels.data() + offset, size_t(expected - offset));
        if (count <= 0) break;
        offset += count;
    }
    close(descriptors[0]);
    if (offset != expected) {
        qCritical("Short screenshot read: expected %lld bytes, received %lld",
                  static_cast<long long>(expected), static_cast<long long>(offset));
        return 1;
    }

    const QImage wrapped(reinterpret_cast<const uchar *>(pixels.constData()), int(width), int(height),
                         int(stride), format);
    if (!wrapped.copy().save(outputPath, "PNG")) {
        qCritical("Could not save screenshot PNG");
        return 1;
    }
    return 0;
}
