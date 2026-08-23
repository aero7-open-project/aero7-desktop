#include "GadgetPainter.h"

#include <QApplication>
#include <QDate>
#include <QLinearGradient>
#include <QLocale>
#include <QPainterPath>
#include <QRadialGradient>
#include <QTimeZone>
#include <QtMath>

namespace {
void roundedPanel(QPainter &p, const QRectF &r, const QColor &top, const QColor &bottom, qreal radius = 5.0)
{
    QPainterPath path;
    path.addRoundedRect(r, radius, radius);
    QLinearGradient gradient(r.topLeft(), r.bottomLeft());
    gradient.setColorAt(0, top);
    gradient.setColorAt(1, bottom);
    p.fillPath(path, gradient);
    p.setPen(QPen(QColor(0, 0, 0, 150), 1));
    p.drawPath(path);
    p.setPen(QPen(QColor(255, 255, 255, 125), 1));
    p.drawRoundedRect(r.adjusted(1, 1, -1, -1), radius - 1, radius - 1);
}

void softShadow(QPainter &p, const QRectF &r, qreal radius = 6.0)
{
    p.setPen(Qt::NoPen);
    for (int i = 5; i > 0; --i) {
        p.setBrush(QColor(0, 0, 0, 8 + i * 3));
        p.drawRoundedRect(r.translated(0, i * 0.7).adjusted(-i * 0.5, -i * 0.2, i * 0.5, i * 0.8), radius, radius);
    }
}

void glossyEllipse(QPainter &p, const QRectF &r, const QColor &center, const QColor &edge)
{
    QRadialGradient radial(r.center() - QPointF(r.width() * .18, r.height() * .22), r.width() * .62);
    radial.setColorAt(0, center.lighter(145));
    radial.setColorAt(.55, center);
    radial.setColorAt(1, edge);
    p.setBrush(radial);
    p.setPen(QPen(QColor(25, 25, 25, 210), qMax(1.0, r.width() / 45.0)));
    p.drawEllipse(r);
    QPainterPath highlight;
    highlight.addEllipse(QRectF(r.left() + r.width() * .17, r.top() + r.height() * .10, r.width() * .66, r.height() * .34));
    p.fillPath(highlight, QColor(255, 255, 255, 55));
}

void drawSun(QPainter &p, const QPointF &center, qreal radius)
{
    p.save();
    p.setPen(QPen(QColor(255, 193, 0, 130), qMax(1.0, radius * .06)));
    for (int i = 0; i < 16; ++i) {
        const qreal angle = qDegreesToRadians(i * 22.5);
        p.drawLine(center + QPointF(qCos(angle), qSin(angle)) * radius * 1.12,
                   center + QPointF(qCos(angle), qSin(angle)) * radius * 1.48);
    }
    QRadialGradient sun(center - QPointF(radius * .2, radius * .25), radius * 1.15);
    sun.setColorAt(0, QColor(255, 255, 184));
    sun.setColorAt(.45, QColor(255, 221, 43));
    sun.setColorAt(1, QColor(245, 157, 0));
    p.setBrush(sun);
    p.setPen(QPen(QColor(255, 174, 0, 180), 1));
    p.drawEllipse(center, radius, radius);
    p.restore();
}

void drawCloud(QPainter &p, const QPointF &center, qreal size)
{
    QLinearGradient cloud(center - QPointF(0, size), center + QPointF(0, size));
    cloud.setColorAt(0, QColor(255, 255, 255, 245));
    cloud.setColorAt(1, QColor(164, 184, 198, 240));
    QPainterPath path;
    path.addEllipse(QRectF(center.x() - size * .52, center.y() - size * .15, size * .70, size * .55));
    path.addEllipse(QRectF(center.x() - size * .17, center.y() - size * .52, size * .72, size * .78));
    path.addEllipse(QRectF(center.x() + size * .16, center.y() - size * .18, size * .65, size * .57));
    path.addRoundedRect(QRectF(center.x() - size * .52, center.y(), size * 1.34, size * .34), size * .12, size * .12);
    p.fillPath(path, cloud);
    p.setPen(QPen(QColor(80, 110, 130, 140), 1));
    p.drawPath(path);
}

void drawWeatherSymbol(QPainter &p, const QPointF &center, qreal radius, int code, bool night = false)
{
    p.save();
    if (code == 0 && night) {
        QRadialGradient moon(center - QPointF(radius * .2, radius * .25), radius * 1.1);
        moon.setColorAt(0, QColor(255, 255, 226)); moon.setColorAt(1, QColor(189, 207, 221));
        p.setBrush(moon); p.setPen(QPen(QColor(113, 139, 160), 1)); p.drawEllipse(center, radius, radius);
        p.setBrush(QColor(117, 191, 225)); p.setPen(Qt::NoPen); p.drawEllipse(center + QPointF(radius * .42, -radius * .22), radius * .82, radius * .82);
        p.restore(); return;
    }
    if (code <= 3) drawSun(p, center - (code == 0 ? QPointF() : QPointF(radius * .25, radius * .20)), radius);
    if (code > 0) drawCloud(p, center + QPointF(radius * .18, radius * .24), radius * .95);
    p.setPen(QPen(QColor(39, 126, 184), qMax(1.3, radius * .08), Qt::SolidLine, Qt::RoundCap));
    if ((code >= 51 && code <= 67) || (code >= 80 && code <= 82)) {
        for (int i = -1; i <= 1; ++i) p.drawLine(center + QPointF(i * radius * .35, radius * .68), center + QPointF(i * radius * .35 - radius * .12, radius * 1.02));
    } else if ((code >= 71 && code <= 77) || (code >= 85 && code <= 86)) {
        p.setPen(Qt::NoPen); p.setBrush(QColor(248, 252, 255));
        for (int i = -1; i <= 1; ++i) p.drawEllipse(center + QPointF(i * radius * .38, radius * .84 + (i % 2) * radius * .10), radius * .09, radius * .09);
    } else if (code >= 95) {
        QPainterPath bolt; bolt.moveTo(center.x() - radius * .08, center.y() + radius * .45); bolt.lineTo(center.x() + radius * .17, center.y() + radius * .45); bolt.lineTo(center.x(), center.y() + radius * .78); bolt.lineTo(center.x() + radius * .19, center.y() + radius * .75); bolt.lineTo(center.x() - radius * .13, center.y() + radius * 1.15); bolt.closeSubpath();
        p.fillPath(bolt, QColor(255, 222, 34));
    } else if (code >= 40 && code <= 48) {
        p.setPen(QPen(QColor(226, 239, 246, 220), qMax(1.2, radius * .08), Qt::SolidLine, Qt::RoundCap));
        for (int i = 0; i < 3; ++i) p.drawLine(center + QPointF(-radius * .65, radius * (.64 + i * .22)), center + QPointF(radius * .72, radius * (.64 + i * .22)));
    }
    p.restore();
}

void drawGauge(QPainter &p, const QRectF &rect, double value, const QString &label)
{
    glossyEllipse(p, rect, QColor(242, 241, 230), QColor(113, 114, 110));
    const QPointF center = rect.center();
    const qreal radius = rect.width() * .39;
    p.setPen(QPen(QColor(45, 45, 42), qMax(1.0, rect.width() / 55.0)));
    for (int i = 0; i <= 10; ++i) {
        const qreal angle = qDegreesToRadians(220.0 + i * 28.0);
        p.drawLine(center + QPointF(qCos(angle), qSin(angle)) * radius * .78,
                   center + QPointF(qCos(angle), qSin(angle)) * radius);
    }
    const qreal needleAngle = qDegreesToRadians(220.0 + qBound(0.0, value, 100.0) * 2.8);
    p.setPen(QPen(QColor(211, 48, 36), qMax(1.5, rect.width() / 35.0), Qt::SolidLine, Qt::RoundCap));
    p.drawLine(center, center + QPointF(qCos(needleAngle), qSin(needleAngle)) * radius * .72);
    p.setBrush(QColor(60, 60, 60));
    p.setPen(Qt::NoPen);
    p.drawEllipse(center, rect.width() * .035, rect.width() * .035);
    p.setPen(QColor(30, 30, 30));
    QFont font(QStringLiteral("Segoe UI"));
    font.setPixelSize(qMax(7, int(rect.width() * .12)));
    font.setBold(true);
    p.setFont(font);
    p.drawText(QRectF(rect.left(), rect.top() + rect.height() * .59, rect.width(), rect.height() * .17), Qt::AlignCenter,
               QString::number(qRound(value)) + QLatin1Char('%'));
    font.setPixelSize(qMax(6, int(rect.width() * .085)));
    p.setFont(font);
    p.drawText(QRectF(rect.left(), rect.top() + rect.height() * .74, rect.width(), rect.height() * .14), Qt::AlignCenter, label);
}

void drawRss(QPainter &p, const QRectF &rect)
{
    p.setPen(QPen(Qt::white, qMax(1.5, rect.width() * .09), Qt::SolidLine, Qt::RoundCap));
    p.setBrush(Qt::white);
    p.drawEllipse(QRectF(rect.left(), rect.bottom() - rect.width() * .18, rect.width() * .18, rect.width() * .18));
    p.drawArc(QRectF(rect.left() - rect.width() * .32, rect.bottom() - rect.width() * .68, rect.width(), rect.width()), 0, 90 * 16);
    p.drawArc(QRectF(rect.left() - rect.width() * .60, rect.bottom() - rect.width() * .98, rect.width() * 1.55, rect.width() * 1.55), 0, 90 * 16);
}

QString weatherText(int code)
{
    if (code == 0) return QStringLiteral("Sunny");
    if (code <= 3) return QStringLiteral("Partly cloudy");
    if (code <= 48) return QStringLiteral("Fog");
    if (code <= 67) return QStringLiteral("Rain");
    if (code <= 77) return QStringLiteral("Snow");
    if (code <= 82) return QStringLiteral("Showers");
    if (code <= 86) return QStringLiteral("Snow showers");
    if (code <= 99) return QStringLiteral("Thunderstorm");
    return QStringLiteral("Weather unavailable");
}

QImage sampleFlowerImage()
{
    QImage image(320, 220, QImage::Format_ARGB32_Premultiplied);
    QPainter painter(&image);
    QLinearGradient background(0, 0, image.width(), image.height());
    background.setColorAt(0, QColor(114, 173, 49));
    background.setColorAt(.48, QColor(246, 218, 43));
    background.setColorAt(1, QColor(45, 116, 48));
    painter.fillRect(image.rect(), background);
    painter.setRenderHint(QPainter::Antialiasing, true);
    const QPointF center(175, 118);
    painter.setPen(Qt::NoPen);
    for (int i = 0; i < 18; ++i) {
        painter.save(); painter.translate(center); painter.rotate(i * 20.0);
        QLinearGradient petal(0, -96, 0, -20); petal.setColorAt(0, QColor(255, 226, 40)); petal.setColorAt(1, QColor(232, 91, 15));
        painter.setBrush(petal); painter.drawEllipse(QRectF(-14, -103, 28, 88)); painter.restore();
    }
    painter.setBrush(QColor(88, 52, 17)); painter.drawEllipse(center, 28, 28);
    return image;
}
}

void GadgetPainter::paint(QPainter &painter, const GadgetDefinition &definition, const GadgetState &state,
                          const GadgetRenderData &data, const QRect &rect)
{
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    if (definition.id.endsWith(QStringLiteral("calendar"))) calendar(painter, state, data, rect);
    else if (definition.id.endsWith(QStringLiteral("clock"))) clock(painter, state, rect);
    else if (definition.id.endsWith(QStringLiteral("cpu"))) cpu(painter, state, data, rect);
    else if (definition.id.endsWith(QStringLiteral("currency"))) currency(painter, state, data, rect);
    else if (definition.id.endsWith(QStringLiteral("feeds"))) feeds(painter, state, data, rect);
    else if (definition.id.endsWith(QStringLiteral("picturepuzzle"))) puzzle(painter, state, data, rect);
    else if (definition.id.endsWith(QStringLiteral("slideshow"))) slideshow(painter, state, data, rect);
    else if (definition.id.endsWith(QStringLiteral("weather"))) weather(painter, state, data, rect);
    else if (definition.id.endsWith(QStringLiteral("mediacenter"))) mediaCenter(painter, state, data, rect);
}

QPixmap GadgetPainter::preview(const GadgetDefinition &definition, const QSize &size)
{
    QPixmap pixmap(size * qApp->devicePixelRatio());
    pixmap.setDevicePixelRatio(qApp->devicePixelRatio());
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    GadgetState state;
    state.id = definition.id;
    state.size = QStringLiteral("small");
    state.settings = {{QStringLiteral("base"), QStringLiteral("EUR")}, {QStringLiteral("target"), QStringLiteral("USD")},
                      {QStringLiteral("amount"), 1.0}, {QStringLiteral("location"), QStringLiteral("Amsterdam")},
                      {QStringLiteral("timezone"), QStringLiteral("Europe/Amsterdam")}};
    GadgetRenderData data;
    data.feedTitles = {QStringLiteral("Aero7 News")};
    if (definition.id.endsWith(QStringLiteral("slideshow"))) data.slideImage = sampleFlowerImage();
    paint(painter, definition, state, data, QRect(QPoint(), size));
    return pixmap;
}

void GadgetPainter::calendar(QPainter &p, const GadgetState &state, const GadgetRenderData &data, const QRect &rect)
{
    const QDate today = QDate::currentDate();
    const QDate date = today.addMonths(data.calendarMonthOffset);
    if (state.size == QStringLiteral("large")) {
        softShadow(p, rect.adjusted(3, 8, -4, -3));
        roundedPanel(p, rect.adjusted(3, 4, -4, -4), QColor(246, 89, 7), QColor(187, 39, 0), 4);
        p.setPen(Qt::white);
        QFont title(QStringLiteral("Segoe UI"), 12, QFont::DemiBold);
        p.setFont(title);
        p.drawText(QRect(rect.left() + 12, rect.top() + 12, rect.width() - 24, 25), Qt::AlignCenter,
                   QLocale().monthName(date.month(), QLocale::LongFormat) + QLatin1Char(' ') + QString::number(date.year()));
        p.setFont(QFont(QStringLiteral("Segoe UI Symbol"), 11, QFont::DemiBold));
        p.drawText(QRect(rect.left() + 8, rect.top() + 11, 28, 25), Qt::AlignCenter, QStringLiteral("‹"));
        p.drawText(QRect(rect.right() - 35, rect.top() + 11, 28, 25), Qt::AlignCenter, QStringLiteral("›"));
        const bool weekNumbers = state.settings.value(QStringLiteral("weekNumbers")).toBool(false);
        const int firstDay = state.settings.value(QStringLiteral("firstDay")).toInt(1);
        const int left = rect.left() + (weekNumbers ? 30 : 14);
        const int top = rect.top() + 48;
        const int cellW = (rect.width() - (weekNumbers ? 44 : 28)) / 7;
        const int cellH = (rect.height() - 62) / 7;
        const QStringList mondayDays{QStringLiteral("Mo"), QStringLiteral("Tu"), QStringLiteral("We"), QStringLiteral("Th"), QStringLiteral("Fr"), QStringLiteral("Sa"), QStringLiteral("Su")};
        const QStringList sundayDays{QStringLiteral("Su"), QStringLiteral("Mo"), QStringLiteral("Tu"), QStringLiteral("We"), QStringLiteral("Th"), QStringLiteral("Fr"), QStringLiteral("Sa")};
        const QStringList &days = firstDay == 7 ? sundayDays : mondayDays;
        QFont grid(QStringLiteral("Segoe UI"), 8);
        p.setFont(grid);
        for (int i = 0; i < 7; ++i) p.drawText(QRect(left + i * cellW, top, cellW, cellH), Qt::AlignCenter, days[i]);
        QDate first(date.year(), date.month(), 1);
        int offset = firstDay == 7 ? first.dayOfWeek() % 7 : first.dayOfWeek() - 1;
        if (weekNumbers) {
            p.setPen(QColor(255, 230, 210));
            p.setFont(QFont(QStringLiteral("Segoe UI"), 7));
            for (int row = 0; row < 6; ++row) {
                const QDate rowDate = first.addDays(row * 7 - offset);
                p.drawText(QRect(rect.left() + 5, top + cellH + row * cellH, 22, cellH), Qt::AlignCenter,
                           QString::number(rowDate.weekNumber()));
            }
        }
        for (int day = 1; day <= date.daysInMonth(); ++day) {
            const int index = offset + day - 1;
            QRect cell(left + (index % 7) * cellW, top + cellH + (index / 7) * cellH, cellW, cellH);
            const bool isToday = state.settings.value(QStringLiteral("highlightToday")).toBool(true)
                && date.year() == today.year() && date.month() == today.month() && day == today.day();
            if (isToday) { p.setBrush(QColor(255, 210, 60)); p.setPen(Qt::NoPen); p.drawEllipse(QPointF(cell.center()), cellH * .42, cellH * .42); }
            p.setPen(isToday ? QColor(120, 25, 0) : Qt::white);
            p.drawText(cell, Qt::AlignCenter, QString::number(day));
        }
        return;
    }

    QRectF body = QRectF(rect).adjusted(4, 10, -4, -3);
    softShadow(p, body);
    roundedPanel(p, body, QColor(255, 91, 0), QColor(201, 43, 0), 3);
    p.setPen(QPen(QColor(36, 36, 36), 2));
    for (int x = int(body.left()) + 9; x < body.right() - 5; x += 11) {
        p.drawLine(QPointF(x, body.top() - 5), QPointF(x, body.top() + 5));
        p.setBrush(QColor(245, 245, 245));
        p.setPen(QPen(QColor(70, 70, 70), 1));
        p.drawEllipse(QPointF(x, body.top() - 4), 2.5, 2.5);
        p.setPen(QPen(QColor(36, 36, 36), 2));
    }
    p.setPen(Qt::white);
    QFont weekday(QStringLiteral("Segoe UI"), 9, QFont::DemiBold);
    p.setFont(weekday);
    p.drawText(QRectF(body.left(), body.top() + 7, body.width(), 18), Qt::AlignCenter,
               QLocale().dayName(date.dayOfWeek(), QLocale::LongFormat));
    QFont number(QStringLiteral("Segoe UI"), qMax(34, int(body.height() * .42)), QFont::Normal);
    number.setLetterSpacing(QFont::AbsoluteSpacing, -2);
    p.setFont(number);
    p.drawText(QRectF(body.left(), body.top() + 22, body.width(), body.height() * .55), Qt::AlignCenter, QString::number(date.day()));
    QFont month(QStringLiteral("Segoe UI"), 9, QFont::DemiBold);
    p.setFont(month);
    p.drawText(QRectF(body.left(), body.bottom() - 26, body.width(), 19), Qt::AlignCenter,
               QLocale().monthName(date.month(), QLocale::LongFormat) + QLatin1Char(' ') + QString::number(date.year()));
}

void GadgetPainter::clock(QPainter &p, const GadgetState &state, const QRect &rect)
{
    const QRectF face = QRectF(rect).adjusted(4, 4, -4, -4);
    softShadow(p, face, face.width() / 2);
    const int faceStyle = state.settings.value(QStringLiteral("face")).toInt(0);
    const QColor faceColor = faceStyle == 1 ? QColor(46, 51, 54) : (faceStyle == 2 ? QColor(227, 236, 243) : QColor(236, 234, 217));
    const QColor rimColor = faceStyle == 1 ? QColor(5, 7, 8) : (faceStyle == 2 ? QColor(62, 91, 112) : QColor(55, 55, 50));
    const QColor inkColor = faceStyle == 1 ? QColor(245, 245, 238) : QColor(25, 25, 23);
    glossyEllipse(p, face, faceColor, rimColor);
    const QPointF center = face.center();
    const qreal radius = face.width() * .405;
    p.setPen(inkColor);
    for (int i = 0; i < 60; ++i) {
        const qreal angle = qDegreesToRadians(i * 6.0 - 90.0);
        const qreal inner = radius * (i % 5 == 0 ? .82 : .92);
        p.setPen(QPen(inkColor, i % 5 == 0 ? qMax(1.5, face.width() / 72.0) : 1.0));
        p.drawLine(center + QPointF(qCos(angle), qSin(angle)) * inner,
                   center + QPointF(qCos(angle), qSin(angle)) * radius);
    }
    QFont numbers(QStringLiteral("Segoe UI"), qMax(8, int(face.width() * .105)), QFont::DemiBold);
    p.setFont(numbers);
    p.setPen(inkColor);
    for (int n = 1; n <= 12; ++n) {
        const qreal angle = qDegreesToRadians(n * 30.0 - 90.0);
        const QPointF pos = center + QPointF(qCos(angle), qSin(angle)) * radius * .66;
        p.drawText(QRectF(pos.x() - 12, pos.y() - 8, 24, 16), Qt::AlignCenter, QString::number(n));
    }
    QTimeZone zone(state.settings.value(QStringLiteral("timezone")).toString(QStringLiteral("Europe/Amsterdam")).toUtf8());
    QTime time = zone.isValid() ? QDateTime::currentDateTime().toTimeZone(zone).time() : QTime::currentTime();
    const qreal second = time.second() * 6.0 - 90.0;
    const qreal minute = (time.minute() + time.second() / 60.0) * 6.0 - 90.0;
    const qreal hour = (time.hour() % 12 + time.minute() / 60.0) * 30.0 - 90.0;
    auto hand = [&](qreal degrees, qreal length, qreal width, const QColor &color) {
        const qreal a = qDegreesToRadians(degrees);
        p.setPen(QPen(color, width, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(center - QPointF(qCos(a), qSin(a)) * radius * .08,
                   center + QPointF(qCos(a), qSin(a)) * radius * length);
    };
    hand(hour, .50, qMax(3.0, face.width() / 28.0), inkColor);
    hand(minute, .72, qMax(2.0, face.width() / 42.0), inkColor);
    if (state.settings.value(QStringLiteral("seconds")).toBool(true)) hand(second, .77, qMax(1.0, face.width() / 85.0), QColor(215, 35, 27));
    p.setBrush(QColor(30, 30, 28)); p.setPen(Qt::NoPen); p.drawEllipse(center, face.width() * .025, face.width() * .025);
    const QString label = state.settings.value(QStringLiteral("label")).toString();
    if (!label.isEmpty()) {
        p.setPen(inkColor);
        p.setFont(QFont(QStringLiteral("Segoe UI"), qMax(6, int(face.width() * .065)), QFont::DemiBold));
        p.drawText(QRectF(face.left() + face.width() * .22, face.top() + face.height() * .69,
                          face.width() * .56, face.height() * .12), Qt::AlignCenter,
                   p.fontMetrics().elidedText(label, Qt::ElideRight, int(face.width() * .56)));
    }
}

void GadgetPainter::cpu(QPainter &p, const GadgetState &state, const GadgetRenderData &data, const QRect &rect)
{
    if (state.size == QStringLiteral("large")) {
        roundedPanel(p, rect.adjusted(2, 2, -2, -2), QColor(42, 48, 51, 235), QColor(5, 8, 10, 240), 6);
        const QRectF cpuRect(rect.left() + 8, rect.top() + 8, rect.height() - 16, rect.height() - 16);
        const QRectF ramRect(rect.right() - rect.height() + 8, rect.top() + 8, rect.height() - 16, rect.height() - 16);
        drawGauge(p, cpuRect, data.cpuPercent, QStringLiteral("CPU"));
        drawGauge(p, ramRect, data.memoryPercent, QStringLiteral("RAM"));
        return;
    }
    const qreal diameter = qMin(rect.height() * .86, rect.width() * .62);
    QRectF left(rect.left() + 2, rect.bottom() - diameter - 2, diameter, diameter);
    QRectF right(rect.right() - diameter - 2, rect.top() + 2, diameter * .82, diameter * .82);
    drawGauge(p, right, data.memoryPercent, QStringLiteral("RAM"));
    drawGauge(p, left, data.cpuPercent, QStringLiteral("CPU"));
}

void GadgetPainter::currency(QPainter &p, const GadgetState &state, const GadgetRenderData &data, const QRect &rect)
{
    const QRectF panel = QRectF(rect).adjusted(2, 2, -2, -2);
    softShadow(p, panel);
    roundedPanel(p, panel, QColor(199, 190, 76), QColor(121, 108, 20), 5);
    p.save();
    p.setClipRect(panel);
    p.setPen(QPen(QColor(75, 74, 31, 85), 1));
    p.drawEllipse(QRectF(panel.left() + 9, panel.top() + 11, panel.width() * .54, panel.height() * .58));
    p.drawLine(QPointF(panel.left() + 14, panel.center().y()), QPointF(panel.right() - 11, panel.center().y()));
    p.drawLine(QPointF(panel.center().x(), panel.top() + 8), QPointF(panel.center().x(), panel.bottom() - 7));
    p.restore();
    const QString base = state.settings.value(QStringLiteral("base")).toString(QStringLiteral("EUR"));
    const QString target = state.settings.value(QStringLiteral("target")).toString(QStringLiteral("USD"));
    const double amount = state.settings.value(QStringLiteral("amount")).toDouble(1.0);
    QFont amountFont(QStringLiteral("Segoe UI"), state.size == QStringLiteral("large") ? 18 : 10, QFont::DemiBold);
    QFont codeFont(QStringLiteral("Segoe UI"), state.size == QStringLiteral("large") ? 10 : 7, QFont::DemiBold);
    p.setPen(Qt::white);
    p.setFont(amountFont);
    const int half = rect.height() / 2;
    p.drawText(QRect(rect.left() + 10, rect.top() + 7, rect.width() - 55, half - 8), Qt::AlignVCenter | Qt::AlignRight, QString::number(amount, 'f', 2));
    p.drawText(QRect(rect.left() + 10, rect.top() + half, rect.width() - 55, half - 7), Qt::AlignVCenter | Qt::AlignRight,
               data.currencyRate > 0 ? QString::number(amount * data.currencyRate, 'f', 2) : QStringLiteral("--"));
    p.setFont(codeFont);
    p.drawText(QRect(rect.right() - 46, rect.top() + 7, 39, half - 8), Qt::AlignCenter, base + QStringLiteral("  ▾"));
    p.drawText(QRect(rect.right() - 46, rect.top() + half, 39, half - 7), Qt::AlignCenter, target + QStringLiteral("  ▾"));
    if (data.currencyRate <= 0.0 && !data.error.isEmpty()) {
        p.setFont(QFont(QStringLiteral("Segoe UI"), state.size == QStringLiteral("large") ? 8 : 6));
        p.setPen(QColor(255, 247, 197));
        p.drawText(QRectF(panel.left() + 5, panel.bottom() - 15, panel.width() - 10, 12), Qt::AlignCenter,
                   QStringLiteral("Cannot connect to service."));
    } else if (state.size == QStringLiteral("large") && !data.currencyUpdated.isEmpty()) {
        p.setFont(QFont(QStringLiteral("Segoe UI"), 7)); p.setPen(QColor(255, 247, 197));
        p.drawText(QRectF(panel.left() + 7, panel.bottom() - 14, panel.width() - 14, 11), Qt::AlignRight,
                   QStringLiteral("Rates updated: ") + data.currencyUpdated);
    }
}

void GadgetPainter::feeds(QPainter &p, const GadgetState &state, const GadgetRenderData &data, const QRect &rect)
{
    const QRectF panel = QRectF(rect).adjusted(2, 2, -2, -2);
    softShadow(p, panel);
    roundedPanel(p, panel, QColor(44, 55, 62, 245), QColor(5, 10, 15, 248), 3);
    const int headerHeight = state.size == QStringLiteral("large") ? 38 : 31;
    p.fillRect(QRectF(panel.left() + 1, panel.top() + 1, panel.width() - 2, headerHeight), QColor(20, 28, 34, 215));
    QRectF rss(panel.right() - headerHeight + 6, panel.top() + 7, headerHeight - 13, headerHeight - 13);
    p.setBrush(QColor(240, 117, 10)); p.setPen(Qt::NoPen); p.drawRoundedRect(rss.adjusted(-3, -3, 3, 3), 2, 2); drawRss(p, rss);
    QFont title(QStringLiteral("Segoe UI"), state.size == QStringLiteral("large") ? 11 : 8, QFont::DemiBold);
    p.setFont(title); p.setPen(Qt::white);
    p.drawText(QRectF(panel.left() + 8, panel.top() + 4, panel.width() - headerHeight - 3, headerHeight - 5), Qt::AlignVCenter, QStringLiteral("Feed Headlines"));
    QStringList titles = data.feedTitles;
    if (titles.isEmpty()) titles << QStringLiteral("View headlines");
    QFont body(QStringLiteral("Segoe UI"), state.size == QStringLiteral("large") ? 9 : 7);
    p.setFont(body);
    const int lineHeight = state.size == QStringLiteral("large") ? 31 : 23;
    const int maxLines = qMax(1, (int(panel.height()) - headerHeight - 15) / lineHeight);
    for (int i = 0; i < qMin(maxLines, titles.size()); ++i) {
        QRectF line(panel.left() + 7, panel.top() + headerHeight + i * lineHeight, panel.width() - 14, lineHeight);
        p.setPen(QColor(255, 255, 255, i == data.feedPage ? 245 : 215));
        p.drawText(line, Qt::AlignVCenter | Qt::TextSingleLine, p.fontMetrics().elidedText(titles.at(i), Qt::ElideRight, int(line.width())));
        p.setPen(QColor(255, 255, 255, 25)); p.drawLine(line.bottomLeft(), line.bottomRight());
    }
    if (!data.error.isEmpty()) {
        p.setPen(QColor(185, 201, 210)); p.setFont(QFont(QStringLiteral("Segoe UI"), state.size == QStringLiteral("large") ? 8 : 6));
        p.drawText(QRectF(panel.left() + 7, panel.bottom() - 15, panel.width() - 14, 12), Qt::AlignLeft | Qt::AlignVCenter,
                   state.size == QStringLiteral("large") ? QStringLiteral("Feed unavailable — using saved headlines")
                                                          : QStringLiteral("Feed unavailable"));
    }
}

void GadgetPainter::puzzle(QPainter &p, const GadgetState &state, const GadgetRenderData &data, const QRect &rect)
{
    const int difficulty = qBound(3, state.settings.value(QStringLiteral("difficulty")).toInt(4), 5);
    const QRect board = rect.adjusted(3, 3, -3, -3);
    roundedPanel(p, board, QColor(68, 52, 30), QColor(17, 14, 12), 2);
    QVector<int> tiles = data.puzzleTiles;
    if (tiles.size() != difficulty * difficulty) {
        for (int i = 0; i < difficulty * difficulty; ++i) tiles << i;
    }
    const QColor palette[] = {QColor(238, 80, 17), QColor(246, 159, 18), QColor(78, 91, 94), QColor(36, 56, 68),
                              QColor(184, 38, 26), QColor(232, 198, 56), QColor(36, 119, 92), QColor(18, 83, 95),
                              QColor(220, 85, 31), QColor(76, 40, 34), QColor(152, 178, 57), QColor(14, 100, 87),
                              QColor(202, 61, 34), QColor(240, 185, 42), QColor(61, 78, 75), QColor(24, 115, 105)};
    const int gap = 2;
    const qreal cellW = (board.width() - 6 - gap * (difficulty - 1)) / qreal(difficulty);
    const qreal cellH = (board.height() - 6 - gap * (difficulty - 1)) / qreal(difficulty);
    for (int index = 0; index < tiles.size(); ++index) {
        const int tile = tiles[index];
        if (tile == difficulty * difficulty - 1) continue;
        QRectF cell(board.left() + 3 + (index % difficulty) * (cellW + gap), board.top() + 3 + (index / difficulty) * (cellH + gap), cellW, cellH);
        if (!data.puzzleImage.isNull()) {
            const int sourceColumn = tile % difficulty;
            const int sourceRow = tile / difficulty;
            const QRect source(sourceColumn * data.puzzleImage.width() / difficulty,
                               sourceRow * data.puzzleImage.height() / difficulty,
                               data.puzzleImage.width() / difficulty,
                               data.puzzleImage.height() / difficulty);
            p.drawImage(cell, data.puzzleImage, source);
            p.setPen(QPen(QColor(255, 241, 190, 150), 1)); p.setBrush(Qt::NoBrush); p.drawRect(cell);
        } else {
            QLinearGradient gradient(cell.topLeft(), cell.bottomRight());
            const QColor color = palette[tile % 16];
            gradient.setColorAt(0, color.lighter(145)); gradient.setColorAt(.45, color); gradient.setColorAt(1, color.darker(150));
            p.setBrush(gradient); p.setPen(QPen(QColor(230, 210, 170, 100), 1)); p.drawRect(cell);
        }
        p.fillRect(QRectF(cell.left() + 2, cell.top() + 2, cell.width() - 4, cell.height() * .22), QColor(255, 255, 255, 35));
        if (data.puzzleImage.isNull() && tile == 6) { p.setBrush(QColor(255, 213, 35)); p.setPen(Qt::NoPen); p.drawEllipse(cell.center(), cellW * .22, cellW * .22); }
    }
}

void GadgetPainter::slideshow(QPainter &p, const GadgetState &, const GadgetRenderData &data, const QRect &rect)
{
    const QRectF frame = QRectF(rect).adjusted(2, 2, -2, -2);
    softShadow(p, frame, 3);
    roundedPanel(p, frame, QColor(56, 61, 61), QColor(5, 7, 8), 3);
    const QRect imageRect = frame.adjusted(5, 5, -5, -5).toRect();
    if (!data.slideImage.isNull()) {
        if (!data.previousSlideImage.isNull() && data.slideTransition < 1.0) {
            p.save(); p.setOpacity(1.0 - data.slideTransition);
            p.drawImage(imageRect, data.previousSlideImage.scaled(imageRect.size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation)); p.restore();
        }
        p.save(); p.setOpacity(data.previousSlideImage.isNull() ? 1.0 : data.slideTransition);
        p.drawImage(imageRect, data.slideImage.scaled(imageRect.size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation)); p.restore();
    } else {
        QLinearGradient background(imageRect.topLeft(), imageRect.bottomRight());
        background.setColorAt(0, QColor(255, 223, 36)); background.setColorAt(1, QColor(113, 145, 32));
        p.fillRect(imageRect, background);
        const QPointF center = imageRect.center();
        p.setBrush(QColor(96, 62, 20)); p.setPen(Qt::NoPen); p.drawEllipse(center, imageRect.height() * .15, imageRect.height() * .15);
        for (int i = 0; i < 14; ++i) {
            const qreal a = qDegreesToRadians(i * (360.0 / 14.0));
            QPainterPath petal;
            petal.addEllipse(QRectF(-imageRect.height() * .08, -imageRect.height() * .34, imageRect.height() * .16, imageRect.height() * .30));
            QTransform transform; transform.translate(center.x(), center.y()); transform.rotateRadians(a);
            p.fillPath(transform.map(petal), QColor(250, 197 - (i % 3) * 18, 15));
        }
    }
    p.setPen(QPen(QColor(255, 255, 255, 110), 1)); p.drawRect(imageRect.adjusted(0, 0, -1, -1));
    if (data.slideshowControlsVisible) {
        const QRectF bar(frame.left() + frame.width() * .20, frame.bottom() - 29,
                         frame.width() * .60, 24);
        QLinearGradient glass(bar.topLeft(), bar.bottomLeft());
        glass.setColorAt(0, QColor(58, 66, 70, 225));
        glass.setColorAt(1, QColor(5, 8, 10, 235));
        p.setPen(QPen(QColor(255, 255, 255, 95), 1));
        p.setBrush(glass);
        p.drawRoundedRect(bar, 7, 7);
        p.setPen(QPen(Qt::white, 1.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        const qreal third = bar.width() / 3.0;
        const QPointF left(bar.left() + third * .5, bar.center().y());
        const QPointF middle(bar.left() + third * 1.5, bar.center().y());
        const QPointF right(bar.left() + third * 2.5, bar.center().y());
        QPainterPath previous; previous.moveTo(left.x() + 4, left.y() - 6); previous.lineTo(left.x() - 3, left.y()); previous.lineTo(left.x() + 4, left.y() + 6); p.drawPath(previous);
        QPainterPath next; next.moveTo(right.x() - 4, right.y() - 6); next.lineTo(right.x() + 3, right.y()); next.lineTo(right.x() - 4, right.y() + 6); p.drawPath(next);
        if (data.slideshowPaused) {
            QPainterPath play; play.moveTo(middle.x() - 3, middle.y() - 6); play.lineTo(middle.x() + 5, middle.y()); play.lineTo(middle.x() - 3, middle.y() + 6); play.closeSubpath(); p.fillPath(play, Qt::white);
        } else {
            p.drawLine(QPointF(middle.x() - 3, middle.y() - 6), QPointF(middle.x() - 3, middle.y() + 6));
            p.drawLine(QPointF(middle.x() + 3, middle.y() - 6), QPointF(middle.x() + 3, middle.y() + 6));
        }
    }
}

void GadgetPainter::weather(QPainter &p, const GadgetState &state, const GadgetRenderData &data, const QRect &rect)
{
    const QRectF panel = QRectF(rect).adjusted(2, 2, -2, -2);
    softShadow(p, panel);
    roundedPanel(p, panel, QColor(218, 246, 255, 238), QColor(76, 174, 218, 236), 7);
    const bool large = state.size == QStringLiteral("large");
    const QString location = state.settings.value(QStringLiteral("location")).toString(QStringLiteral("Doetinchem"));
    QFont locationFont(QStringLiteral("Segoe UI"), large ? 12 : 8, QFont::DemiBold);
    p.setFont(locationFont); p.setPen(QColor(17, 62, 87));
    p.drawText(QRectF(panel.left() + 8, panel.top() + 5, panel.width() - 16, large ? 24 : 17), Qt::AlignLeft | Qt::AlignVCenter, location);
    const QPointF iconCenter(panel.left() + panel.width() * (large ? .29 : .25), panel.top() + panel.height() * (large ? .34 : .58));
    const qreal radius = panel.height() * (large ? .13 : .25);
    const int hour = QTime::currentTime().hour();
    drawWeatherSymbol(p, iconCenter, radius, data.weatherCode, hour < 6 || hour >= 21);
    const bool fahrenheit = state.settings.value(QStringLiteral("unit")).toString() == QStringLiteral("fahrenheit");
    QFont tempFont(QStringLiteral("Segoe UI"), large ? 31 : 19, QFont::Light);
    p.setFont(tempFont); p.setPen(QColor(22, 69, 92));
    p.drawText(QRectF(panel.left() + panel.width() * .49, panel.top() + panel.height() * (large ? .17 : .24), panel.width() * .46, panel.height() * (large ? .27 : .42)),
               Qt::AlignCenter, QString::number(qRound(data.temperature)) + QChar(0x00b0));
    QFont condition(QStringLiteral("Segoe UI"), large ? 10 : 7, QFont::DemiBold);
    p.setFont(condition);
    p.drawText(QRectF(panel.left() + panel.width() * .43, panel.top() + panel.height() * (large ? .42 : .65), panel.width() * .53, panel.height() * (large ? .12 : .22)),
               Qt::AlignCenter, data.error.isEmpty() ? weatherText(data.weatherCode) : QStringLiteral("Weather unavailable"));
    if (large) {
        const qreal forecastTop = panel.top() + panel.height() * .62;
        const qreal columnWidth = panel.width() / 3.0;
        for (int i = 0; i < qMin(3, data.forecastDays.size()); ++i) {
            const QRectF column(panel.left() + i * columnWidth, forecastTop, columnWidth, panel.bottom() - forecastTop - 5);
            p.setPen(QColor(22, 69, 92));
            p.setFont(QFont(QStringLiteral("Segoe UI"), 8, QFont::DemiBold));
            p.drawText(QRectF(column.left(), column.top(), column.width(), 17), Qt::AlignCenter, data.forecastDays.at(i));
            const int code = i < data.forecastCodes.size() ? data.forecastCodes.at(i) : 0;
            const QPointF forecastCenter(column.center().x() - 5, column.top() + 39);
            drawWeatherSymbol(p, forecastCenter, 10, code, false);
            const double temperature = i < data.forecastTemperatures.size() ? data.forecastTemperatures.at(i) : 0.0;
            p.setFont(QFont(QStringLiteral("Segoe UI"), 9, QFont::DemiBold));
            p.drawText(QRectF(column.left(), column.bottom() - 20, column.width(), 18), Qt::AlignCenter,
                       QString::number(qRound(temperature)) + QChar(0x00b0));
        }
    } else if (data.weatherStale && !data.weatherUpdated.isEmpty()) {
        p.setPen(QColor(17, 62, 87, 190));
        p.setFont(QFont(QStringLiteral("Segoe UI"), 6));
        p.drawText(QRectF(panel.left() + 5, panel.bottom() - 12, panel.width() - 10, 10), Qt::AlignRight,
                   QStringLiteral("Updated ") + data.weatherUpdated.section(QLatin1Char('T'), 1, 1).left(5));
    }
    Q_UNUSED(fahrenheit)
}

void GadgetPainter::mediaCenter(QPainter &p, const GadgetState &, const GadgetRenderData &data, const QRect &rect)
{
    const QRectF panel = QRectF(rect).adjusted(2, 2, -2, -2);
    softShadow(p, panel);
    roundedPanel(p, panel, QColor(43, 73, 89, 230), QColor(4, 24, 37, 245), 4);
    const int columns = 6;
    const int rows = 3;
    const qreal tile = qMin(panel.width() / columns, panel.height() * .62 / rows);
    const QColor colors[] = {QColor(71, 126, 150, 180), QColor(178, 104, 70, 180), QColor(101, 139, 91, 180), QColor(78, 92, 118, 180),
                             QColor(146, 154, 96, 180), QColor(63, 118, 157, 180), QColor(46, 87, 105, 180), QColor(105, 76, 94, 180)};
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < columns; ++x) {
            QRectF cell(panel.left() + x * tile, panel.top() + y * tile, tile - 1, tile - 1);
            QLinearGradient gradient(cell.topLeft(), cell.bottomRight());
            QColor color = colors[(x + y * columns) % 8];
            gradient.setColorAt(0, color.lighter(140)); gradient.setColorAt(1, color.darker(140));
            p.fillRect(cell, gradient);
            p.fillRect(QRectF(cell.left(), cell.top(), cell.width(), cell.height() * .28), QColor(255, 255, 255, 22));
        }
    }
    QLinearGradient lower(panel.left(), panel.top() + panel.height() * .52, panel.left(), panel.bottom());
    lower.setColorAt(0, QColor(8, 45, 65, 170)); lower.setColorAt(1, QColor(1, 20, 31, 245));
    p.fillRect(QRectF(panel.left(), panel.top() + panel.height() * .52, panel.width(), panel.height() * .48), lower);
    p.setPen(Qt::white);
    QFont label(QStringLiteral("Segoe UI"), 10);
    p.setFont(label);
    if (!data.mediaArt.isNull()) {
        const QRect artRect(int(panel.left()) + 10, int(panel.top()) + 56, 48, 48);
        p.drawImage(artRect, data.mediaArt.scaled(artRect.size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
        p.setPen(QPen(QColor(255, 255, 255, 100), 1)); p.drawRect(artRect.adjusted(0, 0, -1, -1));
    }
    const int textLeft = data.mediaArt.isNull() ? int(panel.left()) + 16 : int(panel.left()) + 66;
    p.drawText(QRectF(textLeft, panel.top() + panel.height() * .37, panel.right() - textLeft - 12, 20), Qt::AlignLeft | Qt::AlignVCenter,
               data.mediaAvailable ? QStringLiteral("Now Playing") : QStringLiteral("Music"));
    p.drawText(QRectF(textLeft, panel.top() + panel.height() * .51, panel.right() - textLeft - 28, 22), Qt::AlignLeft | Qt::AlignVCenter,
               data.mediaAvailable ? data.mediaTitle : QStringLiteral("Pictures"));
    p.drawText(QRectF(textLeft, panel.top() + panel.height() * .64, panel.right() - textLeft - 28, 22), Qt::AlignLeft | Qt::AlignVCenter,
               data.mediaAvailable ? data.mediaArtist : QStringLiteral("Music + Pictures"));
    const qreal controlsY = panel.top() + panel.height() * .77;
    const qreal controlsCenter = panel.center().x();
    p.setPen(QPen(Qt::white, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    QPainterPath previous; previous.moveTo(controlsCenter - 38, controlsY - 6); previous.lineTo(controlsCenter - 46, controlsY); previous.lineTo(controlsCenter - 38, controlsY + 6); p.drawPath(previous);
    QPainterPath next; next.moveTo(controlsCenter + 38, controlsY - 6); next.lineTo(controlsCenter + 46, controlsY); next.lineTo(controlsCenter + 38, controlsY + 6); p.drawPath(next);
    if (data.mediaPlaying) {
        p.drawLine(QPointF(controlsCenter - 3, controlsY - 6), QPointF(controlsCenter - 3, controlsY + 6));
        p.drawLine(QPointF(controlsCenter + 3, controlsY - 6), QPointF(controlsCenter + 3, controlsY + 6));
    } else {
        QPainterPath play; play.moveTo(controlsCenter - 4, controlsY - 7); play.lineTo(controlsCenter + 6, controlsY); play.lineTo(controlsCenter - 4, controlsY + 7); play.closeSubpath(); p.fillPath(play, Qt::white);
    }
    QFont footer(QStringLiteral("Segoe UI"), 9);
    p.setFont(footer);
    p.drawText(QRectF(panel.left(), panel.bottom() - 29, panel.width(), 26), Qt::AlignCenter, QStringLiteral("Aero7 Media Center"));
    const QPointF orb(panel.right() - 17, panel.bottom() - 17);
    QRadialGradient greenOrb(orb - QPointF(4, 5), 14); greenOrb.setColorAt(0, QColor(240, 255, 173)); greenOrb.setColorAt(.45, QColor(112, 205, 37)); greenOrb.setColorAt(1, QColor(7, 80, 51));
    p.setBrush(greenOrb); p.setPen(QPen(QColor(220, 255, 230, 170), 1)); p.drawEllipse(orb, 13, 13);
    p.setFont(QFont(QStringLiteral("Segoe UI"), 7, QFont::Bold)); p.setPen(Qt::white); p.drawText(QRectF(orb.x() - 10, orb.y() - 9, 20, 18), Qt::AlignCenter, QStringLiteral("A7"));
}
