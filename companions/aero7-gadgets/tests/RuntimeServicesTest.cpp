#include "RuntimeServices.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QUrlQuery>
#include <cstring>
#include <memory>

#include "NetworkFixture.h"

class RuntimeServicesTest final : public QObject
{
    Q_OBJECT
private:
    static QByteArray rss(const QByteArray &title = "Test headline")
    {
        return "<rss version=\"2.0\"><channel><title>QA feed</title><item><title>" + title
            + "</title><link>https://qa.invalid/article</link></item></channel></rss>";
    }
    static QJsonObject weather()
    {
        return {{"current", QJsonObject{{"temperature_2m", 0.0}, {"weather_code", 0}, {"time", "2026-09-10T12:00"}}},
                {"daily", QJsonObject{{"time", QJsonArray{"2026-09-10", "2026-09-11"}},
                                      {"temperature_2m_max", QJsonArray{12.0, 14.0}},
                                      {"weather_code", QJsonArray{1, 3}}}}};
    }
    static QJsonObject rates()
    {
        return {{"date", "2026-09-10"}, {"rates", QJsonObject{{"USD", 1.25}}}};
    }
    static QJsonObject cachedWeather()
    {
        return {{"temperature", 0.0}, {"code", 0}, {"updated", "2026-09-10T12:00"},
                {"forecastDays", QJsonArray{"Thu"}}, {"forecastTemperatures", QJsonArray{12.0}},
                {"forecastCodes", QJsonArray{1}}, {"savedAt", QDateTime::currentSecsSinceEpoch()}};
    }
    void requestWeather() { m_service->requestWeather("QA City", 51.965, 6.288, false); }
    std::unique_ptr<QTemporaryDir> m_profile;
    std::unique_ptr<FixtureNetwork> m_network;
    std::unique_ptr<RuntimeServices> m_service;
private slots:
    void init()
    {
        m_profile = std::make_unique<QTemporaryDir>();
        QVERIFY(m_profile->isValid());
        qputenv("XDG_CACHE_HOME", m_profile->path().toUtf8());
        m_network = std::make_unique<FixtureNetwork>();
        m_service.reset(new RuntimeServices(nullptr, m_network.get()));
        m_service->m_systemTimer.stop();
        m_service->m_mediaTimer.stop();
    }
    void cleanup()
    {
        m_service.reset();
        m_network.reset();
        m_profile.reset();
    }
    void newerRequestWins_data()
    {
        QTest::addColumn<QString>("kind");
        QTest::addColumn<bool>("olderFails");
        for (const QString kind : {QString("weather"), QString("currency"), QString("feed")}) {
            QTest::newRow(qPrintable(kind + "-old-success")) << kind << false;
            QTest::newRow(qPrintable(kind + "-old-failure")) << kind << true;
        }
    }
    void newerRequestWins()
    {
        QFETCH(QString, kind);
        QFETCH(bool, olderFails);
        QSignalSpy weatherSpy(m_service.get(), &RuntimeServices::weatherUpdated);
        QSignalSpy currencySpy(m_service.get(), &RuntimeServices::currencyUpdated);
        QSignalSpy feedSpy(m_service.get(), &RuntimeServices::feedUpdated);
        const auto request = [&]() {
            if (kind == "weather") m_service->requestWeather("QA City", 51.965, 6.288, false, true);
            else if (kind == "currency") m_service->requestCurrency("EUR", "USD", true);
            else m_service->requestFeed("https://qa.invalid/feed", true);
        };
        request();
        request();
        QCOMPARE(m_network->replies.size(), 2);
        auto *older = m_network->replies.first();
        auto *newer = m_network->replies.last();
        if (kind == "weather") {
            auto payload = weather();
            auto current = payload["current"].toObject();
            current["temperature_2m"] = 21.0;
            payload["current"] = current;
            newer->finish(payload);
            older->finish(weather(), olderFails ? QNetworkReply::HostNotFoundError : QNetworkReply::NoError);
            QCOMPARE(weatherSpy.count(), 1);
            QCOMPARE(m_service->readCache("weather", "51.9650,6.2880,0")["temperature"].toDouble(), 21.0);
        } else if (kind == "currency") {
            auto payload = rates();
            payload["rates"] = QJsonObject{{"USD", 2.0}};
            newer->finish(payload);
            older->finish(rates(), olderFails ? QNetworkReply::HostNotFoundError : QNetworkReply::NoError);
            QCOMPARE(currencySpy.count(), 1);
            QCOMPARE(m_service->readCache("currency", "EUR-USD")["rate"].toDouble(), 2.0);
        } else {
            newer->finishBytes(rss("New headline"));
            older->finishBytes(rss("Old headline"), olderFails ? QNetworkReply::HostNotFoundError : QNetworkReply::NoError);
            QCOMPARE(feedSpy.count(), 1);
            QCOMPARE(m_service->readCache("feeds", "https://qa.invalid/feed")["titles"].toArray(), QJsonArray{"New headline"});
        }
    }
    void independentWeatherRequestsBothFinish()
    {
        QSignalSpy spy(m_service.get(), &RuntimeServices::weatherUpdated);
        m_service->requestWeather("First", 51.965, 6.288, false);
        m_service->requestWeather("Second", 40.0, -73.0, false);
        m_network->replies.last()->finish(weather());
        m_network->replies.first()->finish(weather());
        QCOMPARE(spy.count(), 2);
        QVERIFY(!m_service->readCache("weather", "51.9650,6.2880,0").isEmpty());
        QVERIFY(!m_service->readCache("weather", "40.0000,-73.0000,0").isEmpty());
    }
    void feedRejectsBadResponse_data()
    {
        QTest::addColumn<QByteArray>("body");
        QTest::addColumn<bool>("networkError");
        QTest::newRow("failed-request") << rss() << true;
        QTest::newRow("malformed-tail") << (rss() + "<broken>") << false;
        QTest::newRow("unfinished-document") << rss().chopped(6) << false;
        QTest::newRow("wrong-root") << QByteArray("<html><item><title>Not a feed</title></item></html>") << false;
        QTest::newRow("doctype") << (QByteArray("<!DOCTYPE rss [<!ENTITY title 'Injected'>]>") + rss("&title;")) << false;
        QByteArray many = "<rss><channel>";
        for (int i = 0; i < 31; ++i) many += "<item><title>Headline</title></item>";
        QTest::newRow("malformed-after-limit") << (many + "</channel></rss><broken>") << false;
    }
    void feedRejectsBadResponse()
    {
        QFETCH(QByteArray, body);
        QFETCH(bool, networkError);
        QSignalSpy spy(m_service.get(), &RuntimeServices::feedUpdated);
        m_service->requestFeed("https://qa.invalid/feed");
        m_network->replies.last()->finishBytes(body, networkError ? QNetworkReply::ContentAccessDenied : QNetworkReply::NoError);
        QCOMPARE(spy.count(), 1);
        QVERIFY(spy.last()[1].toStringList().isEmpty());
        QVERIFY(!spy.last()[3].toString().isEmpty());
        QVERIFY(m_service->readCache("feeds", "https://qa.invalid/feed").isEmpty());
    }
    void feedSuccessAndCache()
    {
        QSignalSpy spy(m_service.get(), &RuntimeServices::feedUpdated);
        m_service->requestFeed("https://qa.invalid/feed");
        m_network->replies.last()->finishBytes(rss());
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.last()[1].toStringList(), QStringList{"Test headline"});
        QCOMPARE(spy.last()[2].toStringList(), QStringList{"https://qa.invalid/article"});
        QVERIFY(spy.last()[3].toString().isEmpty());
        m_service->requestFeed("https://qa.invalid/feed");
        QCOMPARE(spy.count(), 2);
        QCOMPARE(m_network->replies.size(), 1);
        QCOMPARE(spy.first(), spy.last());
    }
    void atomUsesArticleLink()
    {
        QSignalSpy spy(m_service.get(), &RuntimeServices::feedUpdated);
        m_service->requestFeed("https://qa.invalid/news/feed.xml");
        m_network->replies.last()->finishBytes(
            "<feed xmlns=\"http://www.w3.org/2005/Atom\"><entry><title>Article</title>"
            "<link rel=\"alternate\" href=\"../article\"/><link rel=\"self\" href=\"entry.xml\"/>"
            "<link rel=\"enclosure\" href=\"audio.mp3\"/></entry></feed>");
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.last()[2].toStringList(), QStringList{"https://qa.invalid/article"});
    }
    void feedRetainsCacheOnFailure()
    {
        const QJsonObject cached{{"titles", QJsonArray{"Old headline"}},
                                 {"links", QJsonArray{"https://qa.invalid/article"}}, {"savedAt", 1}};
        m_service->writeCache("feeds", "https://qa.invalid/feed", cached);
        QSignalSpy spy(m_service.get(), &RuntimeServices::feedUpdated);
        m_service->requestFeed("https://qa.invalid/feed");
        QCOMPARE(spy.count(), 1);
        QVERIFY(spy.last()[4].toBool());
        m_network->replies.last()->finishBytes({}, QNetworkReply::HostNotFoundError);
        QCOMPARE(spy.count(), 2);
        QCOMPARE(spy.last()[1].toStringList(), QStringList{"Old headline"});
        QVERIFY(!spy.last()[3].toString().isEmpty());
        QVERIFY(spy.last()[4].toBool());
        QCOMPARE(m_service->readCache("feeds", "https://qa.invalid/feed"), cached);
    }
    void atomBaseAndDefaultRelation()
    {
        QSignalSpy spy(m_service.get(), &RuntimeServices::feedUpdated);
        m_service->requestFeed("https://qa.invalid/feed");
        m_network->replies.last()->finishBytes(
            "<feed xmlns=\"http://www.w3.org/2005/Atom\" xml:base=\"https://news.invalid/base/\">"
            "<entry xml:base=\"section/\"><title type=\"xhtml\"><div xmlns=\"http://www.w3.org/1999/xhtml\">A <b>real</b> title</div></title>"
            "<link rel=\"self\" href=\"entry.xml\"/><link xml:base=\"../\" href=\"article\"/></entry></feed>");
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.last()[1].toStringList(), QStringList{"A real title"});
        QCOMPARE(spy.last()[2].toStringList(), QStringList{"https://news.invalid/base/article"});
        QVERIFY(spy.last()[3].toString().isEmpty());
    }
    void feedCannotOpenLocalLinks()
    {
        QSignalSpy spy(m_service.get(), &RuntimeServices::feedUpdated);
        m_service->requestFeed("https://qa.invalid/feed");
        m_network->replies.last()->finishBytes(
            "<rss><channel><item><title>Unsafe link</title><link>file:///etc/passwd</link></item>"
            "<item><title>No script</title><link>javascript:alert(1)</link></item></channel></rss>");
        QCOMPARE(spy.last()[2].toStringList(), (QStringList{"", ""}));
        QVERIFY(spy.last()[3].toString().isEmpty());
    }
    void oversizedFeedDoesNotCache()
    {
        QSignalSpy spy(m_service.get(), &RuntimeServices::feedUpdated);
        m_service->requestFeed("https://qa.invalid/feed");
        m_network->replies.last()->setProperty("aero7ResponseTooLarge", true);
        m_network->replies.last()->finishBytes(rss());
        QCOMPARE(spy.count(), 1);
        QVERIFY(spy.last()[1].toStringList().isEmpty());
        QVERIFY(!spy.last()[3].toString().isEmpty());
        QVERIFY(m_service->readCache("feeds", "https://qa.invalid/feed").isEmpty());
    }
    void feedLimitsStoredHeadlines()
    {
        QSignalSpy spy(m_service.get(), &RuntimeServices::feedUpdated);
        m_service->requestFeed("https://qa.invalid/feed");
        QByteArray many = "<rss><channel>";
        for (int i = 0; i < 35; ++i) many += "<item><title>Headline</title></item>";
        m_network->replies.last()->finishBytes(many + "</channel></rss>");
        QCOMPARE(spy.last()[1].toStringList().size(), 30);
        QCOMPARE(spy.last()[2].toStringList().size(), 30);
        QVERIFY(spy.last()[3].toString().isEmpty());
    }
    void invalidFeedCacheCannotSuppressRequest()
    {
        m_service->writeCache("feeds", "https://qa.invalid/feed",
                             {{"titles", QJsonArray{42}}, {"links", QJsonArray{}},
                              {"savedAt", QDateTime::currentSecsSinceEpoch()}});
        QSignalSpy spy(m_service.get(), &RuntimeServices::feedUpdated);
        m_service->requestFeed("https://qa.invalid/feed");
        QCOMPARE(spy.count(), 0);
        QCOMPARE(m_network->replies.size(), 1);
    }
    void emptyFeedIsSuccessful()
    {
        QSignalSpy spy(m_service.get(), &RuntimeServices::feedUpdated);
        m_service->requestFeed("https://qa.invalid/feed");
        m_network->replies.last()->finishBytes("<rss version=\"2.0\"><channel><title>Empty</title></channel></rss>");
        QCOMPARE(spy.count(), 1);
        QVERIFY(spy.last()[1].toStringList().isEmpty());
        QVERIFY(spy.last()[3].toString().isEmpty());
        QVERIFY(m_service->readCache("feeds", "https://qa.invalid/feed").contains("titles"));
    }
    void weatherRejectsInvalidReply_data()
    {
        QTest::addColumn<QString>("field");
        QTest::addColumn<QJsonValue>("value");
        QTest::newRow("null-temperature") << QString("temperature_2m") << QJsonValue();
        QTest::newRow("string-temperature") << QString("temperature_2m") << QJsonValue("unknown");
        QTest::newRow("missing-code") << QString("weather_code") << QJsonValue(QJsonValue::Undefined);
        QTest::newRow("fractional-code") << QString("weather_code") << QJsonValue(1.5);
        QTest::newRow("unknown-code") << QString("weather_code") << QJsonValue(100);
        QTest::newRow("unassigned-code") << QString("weather_code") << QJsonValue(4);
        QTest::newRow("missing-time") << QString("time") << QJsonValue(QJsonValue::Undefined);
        QTest::newRow("invalid-time") << QString("time") << QJsonValue("not a date");
    }
    void weatherRejectsInvalidReply()
    {
        QFETCH(QString, field);
        QFETCH(QJsonValue, value);
        QSignalSpy spy(m_service.get(), &RuntimeServices::weatherUpdated);
        requestWeather();
        QCOMPARE(m_network->replies.size(), 1);
        auto payload = weather();
        auto current = payload["current"].toObject();
        current.insert(field, value);
        payload["current"] = current;
        m_network->replies.last()->finish(payload);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.last()[2].toInt(), -1);
        QVERIFY(!spy.last()[5].toString().isEmpty());
        QVERIFY(m_service->readCache("weather", "51.9650,6.2880,0").isEmpty());
    }
    void weatherAcceptsDocumentedCode_data()
    {
        QTest::addColumn<int>("code");
        for (const int code : {0, 1, 2, 3, 45, 48, 51, 53, 55, 56, 57, 61, 63, 65,
                               66, 67, 71, 73, 75, 77, 80, 81, 82, 85, 86, 95, 96, 99}) {
            QTest::newRow(qPrintable(QString::number(code))) << code;
        }
    }
    void weatherAcceptsDocumentedCode()
    {
        QFETCH(int, code);
        QSignalSpy spy(m_service.get(), &RuntimeServices::weatherUpdated);
        requestWeather();
        auto payload = weather();
        auto current = payload["current"].toObject();
        current["weather_code"] = code;
        payload["current"] = current;
        m_network->replies.last()->finish(payload);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.last()[2].toInt(), code);
        QVERIFY(spy.last()[5].toString().isEmpty());
    }
    void weatherSuccessAndCache()
    {
        QSignalSpy spy(m_service.get(), &RuntimeServices::weatherUpdated);
        requestWeather();
        const auto query = QUrlQuery(m_network->replies.last()->url());
        QCOMPARE(query.queryItemValue("latitude"), QString("51.9650"));
        QCOMPARE(query.queryItemValue("temperature_unit"), QString("celsius"));
        m_network->replies.last()->finish(weather());
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.last()[1].toDouble(), 0.0); // A genuine zero must survive.
        QCOMPARE(spy.last()[2].toInt(), 0);
        QVERIFY(spy.last()[5].toString().isEmpty());
        QCOMPARE(spy.last()[7].value<QVector<double>>(), (QVector<double>{12.0, 14.0}));
        requestWeather();
        QCOMPARE(spy.count(), 2);
        QCOMPARE(m_network->replies.size(), 1);
        QCOMPARE(spy.first(), spy.last());
    }
    void weatherDoesNotInventForecast()
    {
        QSignalSpy spy(m_service.get(), &RuntimeServices::weatherUpdated);
        requestWeather();
        auto payload = weather();
        auto daily = payload["daily"].toObject();
        daily["temperature_2m_max"] = QJsonArray{12.0};
        payload["daily"] = daily;
        m_network->replies.last()->finish(payload);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.last()[7].value<QVector<double>>(), QVector<double>{12.0});
        QCOMPARE(spy.last()[6].toStringList().size(), 1);
        QCOMPARE(spy.last()[8].value<QVector<int>>(), QVector<int>{1});
    }
    void weatherRejectsErrorBody()
    {
        QSignalSpy spy(m_service.get(), &RuntimeServices::weatherUpdated);
        requestWeather();
        m_network->replies.last()->finish(weather(), QNetworkReply::ContentAccessDenied);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.last()[2].toInt(), -1);
        QVERIFY(!spy.last()[5].toString().isEmpty());
    }
    void invalidWeatherCacheCannotSuppressRequest()
    {
        auto cached = cachedWeather();
        cached["temperature"] = QJsonValue();
        m_service->writeCache("weather", "51.9650,6.2880,0", cached);
        QSignalSpy spy(m_service.get(), &RuntimeServices::weatherUpdated);
        requestWeather();
        QCOMPARE(spy.count(), 0);
        QCOMPARE(m_network->replies.size(), 1);
        m_network->replies.last()->finish(weather());
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.last()[2].toInt(), 0);
    }
    void staleWeatherSurvivesFailedRefresh()
    {
        auto cached = cachedWeather();
        cached["savedAt"] = QDateTime::currentSecsSinceEpoch() - 3600;
        m_service->writeCache("weather", "51.9650,6.2880,0", cached);
        QSignalSpy spy(m_service.get(), &RuntimeServices::weatherUpdated);
        requestWeather();
        QCOMPARE(spy.count(), 1);
        QVERIFY(spy.last()[4].toBool());
        m_network->replies.last()->finish({}, QNetworkReply::HostNotFoundError);
        QCOMPARE(spy.count(), 2);
        QCOMPARE(spy.last()[1].toDouble(), 0.0);
        QCOMPARE(spy.last()[2].toInt(), 0);
        QVERIFY(spy.last()[4].toBool());
        QVERIFY(!spy.last()[5].toString().isEmpty());
        QCOMPARE(m_service->readCache("weather", "51.9650,6.2880,0"), cached);
    }
    void cachedForecastCannotInventValues()
    {
        auto cached = cachedWeather();
        cached["forecastTemperatures"] = QJsonArray{QJsonValue()};
        m_service->writeCache("weather", "51.9650,6.2880,0", cached);
        QSignalSpy spy(m_service.get(), &RuntimeServices::weatherUpdated);
        requestWeather();
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.last()[2].toInt(), 0);
        QVERIFY(spy.last()[6].toStringList().isEmpty());
        QVERIFY(spy.last()[7].value<QVector<double>>().isEmpty());
        QVERIFY(spy.last()[8].value<QVector<int>>().isEmpty());
    }
    void oversizedReplyCannotUpdateCache()
    {
        QSignalSpy spy(m_service.get(), &RuntimeServices::weatherUpdated);
        requestWeather();
        m_network->replies.last()->setProperty("aero7ResponseTooLarge", true);
        m_network->replies.last()->finish(weather());
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.last()[2].toInt(), -1);
        QVERIFY(!spy.last()[5].toString().isEmpty());
        QVERIFY(m_service->readCache("weather", "51.9650,6.2880,0").isEmpty());
    }
    void futureCacheIsNotFresh()
    {
        auto cached = cachedWeather();
        cached["savedAt"] = QDateTime::currentSecsSinceEpoch() + 86400;
        QVERIFY(!m_service->cacheFresh(cached, 1800));
    }
    void currencyRejectsErrorBody()
    {
        QSignalSpy spy(m_service.get(), &RuntimeServices::currencyUpdated);
        m_service->requestCurrency("EUR", "USD");
        m_network->replies.last()->finish(rates(), QNetworkReply::ContentAccessDenied);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.last()[2].toDouble(), 0.0);
        QVERIFY(!spy.last()[5].toString().isEmpty());
        QVERIFY(m_service->readCache("currency", "EUR-USD").isEmpty());
    }
    void currencySuccessAndCache()
    {
        QSignalSpy spy(m_service.get(), &RuntimeServices::currencyUpdated);
        m_service->requestCurrency("eur", "usd");
        m_network->replies.last()->finish(rates());
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.last()[0].toString(), QString("EUR"));
        QCOMPARE(spy.last()[2].toDouble(), 1.25);
        m_service->requestCurrency("EUR", "USD");
        QCOMPARE(m_network->replies.size(), 1);
        QCOMPARE(spy.count(), 2);
        QCOMPARE(spy.first(), spy.last());
    }
    void currencyRequiresDate()
    {
        QSignalSpy spy(m_service.get(), &RuntimeServices::currencyUpdated);
        m_service->requestCurrency("EUR", "USD");
        auto payload = rates();
        payload.remove("date");
        m_network->replies.last()->finish(payload);
        QCOMPARE(spy.count(), 1);
        QVERIFY(!spy.last()[5].toString().isEmpty());
        QVERIFY(m_service->readCache("currency", "EUR-USD").isEmpty());
    }
    void staleCurrencySurvivesFailedRefresh()
    {
        const QJsonObject cached{{"rate", 1.25}, {"updated", "2026-09-09"},
                                 {"savedAt", QDateTime::currentSecsSinceEpoch() - 86400}};
        m_service->writeCache("currency", "EUR-USD", cached);
        QSignalSpy spy(m_service.get(), &RuntimeServices::currencyUpdated);
        m_service->requestCurrency("EUR", "USD");
        QCOMPARE(spy.count(), 1);
        m_network->replies.last()->finish({}, QNetworkReply::HostNotFoundError);
        QCOMPARE(spy.count(), 2);
        QCOMPARE(spy.last()[2].toDouble(), 1.25);
        QVERIFY(spy.last()[4].toBool());
        QVERIFY(!spy.last()[5].toString().isEmpty());
        QCOMPARE(m_service->readCache("currency", "EUR-USD"), cached);
    }
    void sameCurrencyNeedsNoNetwork()
    {
        QSignalSpy spy(m_service.get(), &RuntimeServices::currencyUpdated);
        m_service->requestCurrency("eur", "EUR");
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.last()[2].toDouble(), 1.0);
        QVERIFY(spy.last()[5].toString().isEmpty());
        QVERIFY(m_network->replies.isEmpty());
    }
    void invalidCurrencyCacheCannotSuppressRequest()
    {
        m_service->writeCache("currency", "EUR-USD", {{"rate", -1}, {"updated", "2026-09-10"},
                                                       {"savedAt", QDateTime::currentSecsSinceEpoch()}});
        QSignalSpy spy(m_service.get(), &RuntimeServices::currencyUpdated);
        m_service->requestCurrency("EUR", "USD");
        QCOMPARE(spy.count(), 0);
        QCOMPARE(m_network->replies.size(), 1);
    }
};

QTEST_GUILESS_MAIN(RuntimeServicesTest)
#include "RuntimeServicesTest.moc"
