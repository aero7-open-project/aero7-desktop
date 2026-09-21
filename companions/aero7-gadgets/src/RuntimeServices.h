#pragma once

#include <QObject>
#include <QImage>
#include <QNetworkAccessManager>
#include <QTimer>
#include <QPointer>
#include <QHash>

class RuntimeServices final : public QObject
{
    Q_OBJECT

public:
    static RuntimeServices *instance();
    static QString weatherRequestKey(double latitude, double longitude, bool fahrenheit);

    void requestCurrency(const QString &base, const QString &target, bool force = false);
    void requestWeather(const QString &location, double latitude, double longitude, bool fahrenheit, bool force = false);
    void requestFeed(const QString &url, bool force = false);
    void mediaCommand(const QString &method);

signals:
    void systemUpdated(double cpuPercent, double memoryPercent, quint64 usedBytes, quint64 totalBytes);
    void currencyUpdated(const QString &base, const QString &target, double rate,
                         const QString &updated, bool stale, const QString &error);
    void weatherUpdated(const QString &location, double temperature, int weatherCode,
                        const QString &updated, bool stale, const QString &error,
                        const QStringList &forecastDays, const QVector<double> &forecastTemperatures,
                        const QVector<int> &forecastCodes, const QString &requestKey);
    void feedUpdated(const QString &url, const QStringList &titles, const QStringList &links,
                     const QString &error, bool stale = false);
    void mediaUpdated(const QString &title, const QString &artist, const QString &album,
                      const QString &artUrl, bool playing, bool available);
    void mediaArtUpdated(const QString &artUrl, const QImage &image);

private slots:
    void updateSystem();
    void updateMedia();

private:
    friend class RuntimeServicesTest;
    friend class GalleryTest;
    explicit RuntimeServices(QObject *parent = nullptr, QNetworkAccessManager *network = nullptr);
    QString cacheFile(const QString &category, const QString &key) const;
    QJsonObject readCache(const QString &category, const QString &key) const;
    void writeCache(const QString &category, const QString &key, const QJsonObject &object) const;
    bool cacheFresh(const QJsonObject &cache, qint64 maximumAgeSeconds) const;
    void publishWeather(const QString &location, const QString &key, const QJsonObject &reading, bool stale, const QString &error);
    bool acceptReply(const QString &key, QNetworkReply *reply);

    QNetworkAccessManager *m_network;
    QHash<QString, QPointer<QNetworkReply>> m_latestReplies;
    QTimer m_systemTimer;
    QTimer m_mediaTimer;
    quint64 m_previousCpuTotal = 0;
    quint64 m_previousCpuIdle = 0;
    QString m_activePlayer;
    QString m_lastArtUrl;
    QImage m_mediaArt;
};
