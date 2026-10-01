#pragma once

#include <QBuffer>
#include <QImage>
#include <QMimeData>
#include <memory>

inline std::unique_ptr<QMimeData> captureClipboardData(const QImage &image)
{
    if (image.isNull())
        return {};
    QByteArray png;
    QBuffer buffer(&png);
    if (!buffer.open(QIODevice::WriteOnly) || !image.save(&buffer, "PNG"))
        return {};

    auto data = std::make_unique<QMimeData>();
    data->setImageData(image);
    data->setData(QStringLiteral("image/png"), png);
    // Use Spectacle's explicit-copy handoff: Klipper retains this screenshot
    // after the producer exits, without changing global image-history settings.
    // Copied screenshots can consequently appear in the user's clipboard history.
    data->setData(QStringLiteral("x-kde-force-image-copy"), QByteArray());
    return data;
}
