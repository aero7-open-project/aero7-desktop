#include "../CaptureClipboardData.h"
#include <QCoreApplication>
#include <QDebug>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    if (captureClipboardData(QImage())) {
        qCritical() << "An invalid image must not replace the clipboard";
        return 1;
    }
    QImage source(7, 5, QImage::Format_ARGB32);
    for (int y = 0; y < source.height(); ++y)
        for (int x = 0; x < source.width(); ++x)
            source.setPixel(x, y, qRgba(x * 35, y * 51, (x + y) * 21, 127 + x * 20));
    const auto data = captureClipboardData(source);
    if (!data || !data->hasImage() || !data->hasFormat(QStringLiteral("image/png"))
        || !data->hasFormat(QStringLiteral("x-kde-force-image-copy"))
        || data->hasText() || data->hasUrls()) {
        qCritical() << "Explicit image-copy formats missing, or path copied instead";
        return 2;
    }
    const QImage png = QImage::fromData(data->data(QStringLiteral("image/png")), "PNG");
    const QImage native = qvariant_cast<QImage>(data->imageData());
    if (png != source || native != source) {
        qCritical() << "Clipboard encoding changed screenshot pixels";
        return 3;
    }
    return 0;
}
