#pragma once

#include "GadgetTypes.h"

#include <QDateTime>
#include <QImage>
#include <QPainter>
#include <QStringList>
#include <QVector>

struct GadgetRenderData {
    double cpuPercent = 27.0;
    double memoryPercent = 53.0;
    quint64 memoryUsed = 0;
    quint64 memoryTotal = 0;
    double currencyRate = 1.17;
    QString currencyUpdated;
    bool currencyStale = false;
    double temperature = 20.0;
    int weatherCode = 1;
    QString weatherUpdated;
    bool weatherStale = false;
    QString error;
    QStringList feedTitles;
    QStringList feedLinks;
    int feedPage = 0;
    QImage slideImage;
    QImage previousSlideImage;
    qreal slideTransition = 1.0;
    QImage puzzleImage;
    QVector<int> puzzleTiles;
    int puzzleMoves = 0;
    QString mediaTitle = QStringLiteral("No media playing");
    QString mediaArtist;
    QString mediaAlbum;
    QImage mediaArt;
    bool mediaPlaying = false;
    bool mediaAvailable = false;
    int calendarMonthOffset = 0;
    bool slideshowControlsVisible = false;
    bool slideshowPaused = false;
    QStringList forecastDays;
    QVector<double> forecastTemperatures;
    QVector<int> forecastCodes;
};

class GadgetPainter
{
public:
    static void paint(QPainter &painter, const GadgetDefinition &definition, const GadgetState &state,
                      const GadgetRenderData &data, const QRect &rect);
    static QPixmap preview(const GadgetDefinition &definition, const QSize &size);

private:
    static void calendar(QPainter &, const GadgetState &, const GadgetRenderData &, const QRect &);
    static void clock(QPainter &, const GadgetState &, const QRect &);
    static void cpu(QPainter &, const GadgetState &, const GadgetRenderData &, const QRect &);
    static void currency(QPainter &, const GadgetState &, const GadgetRenderData &, const QRect &);
    static void feeds(QPainter &, const GadgetState &, const GadgetRenderData &, const QRect &);
    static void puzzle(QPainter &, const GadgetState &, const GadgetRenderData &, const QRect &);
    static void slideshow(QPainter &, const GadgetState &, const GadgetRenderData &, const QRect &);
    static void weather(QPainter &, const GadgetState &, const GadgetRenderData &, const QRect &);
    static void mediaCenter(QPainter &, const GadgetState &, const GadgetRenderData &, const QRect &);
};
