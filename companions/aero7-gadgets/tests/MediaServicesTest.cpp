#include "RuntimeServices.h"
#include "NetworkFixture.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QDBusConnection>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSaveFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <memory>

// A separate process exports real MPRIS properties and methods on the private
// test bus. Production polling/command code is not replaced by a fake bus API.
class FixturePlayer final : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2.Player")
    Q_PROPERTY(QString PlaybackStatus READ playbackStatus)
    Q_PROPERTY(QVariantMap Metadata READ metadata)
public:
    explicit FixturePlayer(QString stateFile) : m_stateFile(std::move(stateFile)) {}
    QJsonObject state() const
    {
        QFile file(m_stateFile);
        if (!file.open(QIODevice::ReadOnly)) return {};
        return QJsonDocument::fromJson(file.readAll()).object();
    }
    QString playbackStatus() const { return state()["status"].toString(); }
    QVariantMap metadata() const
    {
        const auto value = state();
        return {{"xesam:title", value["title"].toString()},
                {"xesam:artist", QStringList{QStringLiteral("QA Artist")}},
                {"xesam:album", QStringLiteral("QA Album")},
                {"mpris:artUrl", value["art"].toString()}};
    }
public slots:
    void PlayPause() { record("PlayPause"); }
    void Previous() { record("Previous"); }
    void Next() { record("Next"); }
private:
    void record(const QByteArray &method)
    {
        QFile file(m_stateFile + ".commands");
        if (file.open(QIODevice::WriteOnly | QIODevice::Append)) file.write(method + '\n');
    }
    QString m_stateFile;
};

// This target intentionally uses the existing narrow test-friend name.
class RuntimeServicesTest final : public QObject
{
    Q_OBJECT
    std::unique_ptr<QTemporaryDir> m_profile;
    std::unique_ptr<FixtureNetwork> m_network;
    std::unique_ptr<RuntimeServices> m_service;
    std::unique_ptr<QProcess> m_player;
    QString m_stateFile;
    QString m_art;

    void writeState(const QString &art, const QString &title = "QA Track",
                    const QString &status = "Playing")
    {
        QSaveFile file(m_stateFile);
        QVERIFY(file.open(QIODevice::WriteOnly));
        const QByteArray data = QJsonDocument(QJsonObject{{"art", art}, {"title", title},
                                                         {"status", status}}).toJson();
        QCOMPARE(file.write(data), data.size());
        QVERIFY(file.commit());
    }
    static QByteArray imageBytes()
    {
        QImage image(8, 8, QImage::Format_ARGB32);
        image.fill(Qt::red);
        QByteArray bytes;
        QBuffer buffer(&bytes);
        buffer.open(QIODevice::WriteOnly);
        image.save(&buffer, "PNG");
        return bytes;
    }
private slots:
    void init()
    {
        QVERIFY(QDBusConnection::sessionBus().isConnected());
        m_profile = std::make_unique<QTemporaryDir>();
        QVERIFY(m_profile->isValid());
        m_stateFile = m_profile->filePath("player.json");
        const QString imagePath = m_profile->filePath("cover.png");
        QFile image(imagePath);
        QVERIFY(image.open(QIODevice::WriteOnly));
        const auto bytes = imageBytes();
        QCOMPARE(image.write(bytes), bytes.size());
        image.close();
        m_art = QUrl::fromLocalFile(imagePath).toString();
        // Constructor polling must not consume the cover before spies attach.
        writeState({});
        m_player = std::make_unique<QProcess>();
        m_player->start(QCoreApplication::applicationFilePath(), {"--fixture-player", m_stateFile});
        QVERIFY(m_player->waitForStarted());
        QVERIFY(m_player->waitForReadyRead());
        QCOMPARE(m_player->readLine(), QByteArray("READY\n"));
        qputenv("XDG_CACHE_HOME", m_profile->path().toUtf8());
        m_network = std::make_unique<FixtureNetwork>();
        m_service.reset(new RuntimeServices(nullptr, m_network.get()));
        m_service->m_systemTimer.stop();
        m_service->m_mediaTimer.stop();
        writeState(m_art);
    }
    void cleanup()
    {
        m_service.reset();
        m_network.reset();
        if (m_player && m_player->state() != QProcess::NotRunning) {
            m_player->terminate();
            if (!m_player->waitForFinished(2000)) {
                m_player->kill();
                m_player->waitForFinished();
            }
        }
        m_player.reset();
        m_profile.reset();
    }
    void metadataAndCommands()
    {
        QSignalSpy media(m_service.get(), &RuntimeServices::mediaUpdated);
        m_service->updateMedia();
        QCOMPARE(media.size(), 1);
        QCOMPARE(media.last()[0].toString(), QString("QA Track"));
        QCOMPARE(media.last()[1].toString(), QString("QA Artist"));
        QCOMPARE(media.last()[2].toString(), QString("QA Album"));
        QVERIFY(media.last()[4].toBool());
        QVERIFY(media.last()[5].toBool());
        for (const auto &command : {"Previous", "PlayPause", "Next", "Quit"})
            m_service->mediaCommand(command);
        const auto commands = [&]() {
            QFile file(m_stateFile + ".commands");
            return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
        };
        QTRY_COMPARE(commands(), QByteArray("Previous\nPlayPause\nNext\n"));
        writeState(m_art, "Paused Track", "Paused");
        m_service->updateMedia();
        QCOMPARE(media.last()[0].toString(), QString("Paused Track"));
        QVERIFY(!media.last()[4].toBool());
        m_player->terminate();
        QVERIFY(m_player->waitForFinished());
        m_service->updateMedia();
        QVERIFY(!media.last()[5].toBool());
    }
    void lateSubscriberReceivesCurrentCover()
    {
        m_service->updateMedia();
        // A newly created gadget connects after the service's first poll.
        QSignalSpy covers(m_service.get(), &RuntimeServices::mediaArtUpdated);
        m_service->updateMedia();
        QVERIFY2(!covers.isEmpty(), "New subscriber never receives the current album cover");
        QVERIFY(!qvariant_cast<QImage>(covers.last()[1]).isNull());
        QCOMPARE(m_network->replies.size(), 0);
    }
    void changedTrackClearsOldCover_data()
    {
        QTest::addColumn<QString>("art");
        QTest::newRow("no-cover") << QString();
        QTest::newRow("missing-file") << QString("file:///nonexistent-aero7-qa-cover.png");
        QTest::newRow("unsupported-scheme") << QString("ftp://qa.invalid/cover.png");
        QTest::newRow("pending-download") << QString("https://qa.invalid/cover.png");
    }
    void changedTrackClearsOldCover()
    {
        QFETCH(QString, art);
        QSignalSpy covers(m_service.get(), &RuntimeServices::mediaArtUpdated);
        m_service->updateMedia();
        QVERIFY(!covers.isEmpty());
        QVERIFY(!qvariant_cast<QImage>(covers.last()[1]).isNull());
        covers.clear();
        writeState(art, "Different Track");
        m_service->updateMedia();
        QVERIFY2(!covers.isEmpty(), "Track changed but previous album cover was not cleared");
        QVERIFY(qvariant_cast<QImage>(covers.last()[1]).isNull());
    }
    void oldDownloadCannotReplaceCoverlessTrack()
    {
        writeState("https://qa.invalid/old.png", "Old Track");
        QSignalSpy covers(m_service.get(), &RuntimeServices::mediaArtUpdated);
        m_service->updateMedia();
        QCOMPARE(m_network->replies.size(), 1);
        writeState({}, "Coverless Track");
        m_service->updateMedia();
        covers.clear();
        m_network->replies.first()->finishBytes(imageBytes());
        for (const auto &event : covers)
            QVERIFY2(qvariant_cast<QImage>(event[1]).isNull(), "Old track download replaced coverless track");
    }
    void sameUrlReturnRejectsFirstRequest()
    {
        const QString url = "https://qa.invalid/reused.png";
        writeState(url, "First Track");
        m_service->updateMedia();
        writeState("https://qa.invalid/middle.png", "Middle Track");
        m_service->updateMedia();
        writeState(url, "Latest Track");
        m_service->updateMedia();
        QCOMPARE(m_network->replies.size(), 3);
        QSignalSpy covers(m_service.get(), &RuntimeServices::mediaArtUpdated);
        m_network->replies.last()->finishBytes(imageBytes());
        QCOMPARE(covers.size(), 1);
        QVERIFY(!qvariant_cast<QImage>(covers.last()[1]).isNull());
        covers.clear();
        m_network->replies.first()->finishBytes(imageBytes());
        QVERIFY2(covers.isEmpty(), "Superseded request with reused URL was accepted");
    }
    void failedDownloadRejectsImagePayload()
    {
        writeState("https://qa.invalid/error.png");
        m_service->updateMedia();
        QCOMPARE(m_network->replies.size(), 1);
        QSignalSpy covers(m_service.get(), &RuntimeServices::mediaArtUpdated);
        m_network->replies.first()->finishBytes(imageBytes(), QNetworkReply::ContentAccessDenied);
        QVERIFY2(covers.isEmpty(), "Failed HTTP response was accepted as album art");
    }
};

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    if (application.arguments().value(1) == "--fixture-player") {
        FixturePlayer player(application.arguments().value(2));
        auto bus = QDBusConnection::sessionBus();
        if (!bus.registerObject("/org/mpris/MediaPlayer2", &player,
                                QDBusConnection::ExportAllProperties | QDBusConnection::ExportAllSlots)
            || !bus.registerService("org.mpris.MediaPlayer2.Aero7QATest")) return 2;
        QFile output;
        if (!output.open(stdout, QIODevice::WriteOnly)) return 3;
        output.write("READY\n");
        output.flush();
        return application.exec();
    }
    RuntimeServicesTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "MediaServicesTest.moc"
