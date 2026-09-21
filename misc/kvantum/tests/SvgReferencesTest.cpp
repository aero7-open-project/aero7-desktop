#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QSet>
#include <QSvgRenderer>
#include <QXmlStreamReader>

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    if (argc != 2)
        return 1;
    QFile file(QString::fromLocal8Bit(argv[1]));
    if (!file.open(QIODevice::ReadOnly))
        return 2;
    const QByteArray data = file.readAll();
    QSet<QString> ids;
    QStringList links;
    QXmlStreamReader xml(data);
    while (!xml.atEnd()) {
        xml.readNext();
        if (!xml.isStartElement())
            continue;
        for (const auto &attribute : xml.attributes()) {
            if (attribute.name() == QLatin1String("id"))
                ids.insert(attribute.value().toString());
            if (attribute.name() == QLatin1String("href") && attribute.value().startsWith(QLatin1Char('#')))
                links.append(attribute.value().mid(1).toString());
        }
    }
    if (xml.hasError())
        return 3;
    for (const auto &link : links) {
        if (!ids.contains(link)) {
            qCritical("Unresolved SVG reference: %s", qPrintable(link));
            return 4;
        }
    }
    QSvgRenderer renderer(data);
    if (!renderer.isValid())
        return 5;
    for (const auto &id : {QStringLiteral("slider-normal-bottomleft"),
                           QStringLiteral("slider-normal-bottomright")}) {
        if (!renderer.elementExists(id) || renderer.boundsOnElement(id).isEmpty())
            return 6;
        QImage image(16, 16, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        renderer.render(&painter, id, QRectF(0, 0, 16, 16));
        painter.end();
        bool hasPixels = false;
        for (int y = 0; y < image.height(); ++y)
            for (int x = 0; x < image.width(); ++x)
                hasPixels |= qAlpha(image.pixel(x, y)) != 0;
        if (!hasPixels)
            return 7;
    }
    qInfo("All %lld local SVG references resolve; both lower slider corners render.", qlonglong(links.size()));
    return 0;
}
