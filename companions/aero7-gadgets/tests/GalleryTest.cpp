#include "GadgetGallery.h"
#include "GadgetManager.h"
#include "GadgetWindow.h"
#include "GadgetOptionsDialog.h"
#include "RuntimeServices.h"
#include "NetworkFixture.h"

#include <LayerShellQt/Window>
#include <KWindowSystem>

#include <QApplication>
#include <QBuffer>
#include <QComboBox>
#include <QCheckBox>
#include <QDesktopServices>
#include <QDoubleSpinBox>
#include <QDir>
#include <QFile>
#include <QInputDialog>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMouseEvent>
#include <QPushButton>
#include <QProcess>
#include <QScopeGuard>
#include <QSpinBox>
#include <QSignalSpy>
#include <QStyle>
#include <QSvgGenerator>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QToolButton>
#include <cmath>
#include <limits>
#include <memory>

namespace {
class UpdateCounter final : public QObject
{
public:
    int requests = 0;
    bool eventFilter(QObject *, QEvent *event) override
    {
        if (event->type() == QEvent::UpdateRequest) ++requests;
        return false;
    }
};

double luminance(const QColor &color)
{
    const auto linear = [](double channel) {
        return channel <= 0.04045 ? channel / 12.92 : std::pow((channel + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * linear(color.redF()) + 0.7152 * linear(color.greenF()) + 0.0722 * linear(color.blueF());
}

double contrast(const QColor &first, const QColor &second)
{
    const double a = luminance(first), b = luminance(second);
    return (std::max(a, b) + 0.05) / (std::min(a, b) + 0.05);
}
}

class GalleryTest final : public QObject
{
    Q_OBJECT
private slots:
    void recordOpenedUrl(const QUrl &url) { m_openedUrls << url; }

    void feedManagerValidatesAddedUrl_data()
    {
        QTest::addColumn<QString>("url");
        QTest::addColumn<bool>("valid");
        QTest::addColumn<bool>("cancel");
        QTest::newRow("https") << QString("https://qa.invalid/feed.xml") << true << false;
        QTest::newRow("http-port-query") << QString("http://localhost:8080/feed?category=one&limit=2") << true << false;
        QTest::newRow("ipv6") << QString("https://[::1]/feed.atom") << true << false;
        QTest::newRow("trim-whitespace") << QString("  https://qa.invalid/feed.atom  ") << true << false;
        QTest::newRow("empty-host") << QString("https://") << false << false;
        QTest::newRow("invalid-ipv6") << QString("https://[broken/feed") << false << false;
        QTest::newRow("invalid-port") << QString("https://qa.invalid:invalid/feed") << false << false;
        QTest::newRow("invalid-percent-encoding") << QString("https://qa.invalid/%ZZ") << false << false;
        QTest::newRow("missing-scheme") << QString("qa.invalid/feed.xml") << false << false;
        QTest::newRow("file-url") << QString("file:///tmp/feed.xml") << false << false;
        QTest::newRow("empty") << QString("") << false << false;
        QTest::newRow("cancel") << QString("https://qa.invalid/feed.xml") << false << true;
    }
    void feedManagerValidatesAddedUrl()
    {
        QFETCH(QString, url);
        QFETCH(bool, valid);
        QFETCH(bool, cancel);
        QTemporaryDir profile;
        QVERIFY(profile.isValid());
        const auto oldConfig = qgetenv("XDG_CONFIG_HOME");
        qputenv("XDG_CONFIG_HOME", profile.path().toUtf8());
        const auto restoreConfig = qScopeGuard([&]() { qputenv("XDG_CONFIG_HOME", oldConfig); });
        const auto *definition = m_manager->definition("org.aero7.gadgets.feeds");
        QVERIFY(definition);
        GadgetOptionsDialog options(*definition, {});
        QPushButton *manage = nullptr;
        for (auto *button : options.findChildren<QPushButton *>()) if (button->text() == "Manage feeds...") manage = button;
        QVERIFY(manage);
        int initialCount = -1, finalCount = -1, inputs = 0;
        bool errorVisible = false;
        QString savedUrl;
        QTimer::singleShot(0, &options, [&]() {
            auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            if (!dialog || dialog->windowTitle() != "Manage Feeds") return;
            auto *list = dialog->findChild<QListWidget *>();
            QPushButton *add = nullptr;
            for (auto *button : dialog->findChildren<QPushButton *>()) if (button->text() == "Add...") add = button;
            if (list && add) {
                initialCount = list->count();
                QTimer inputDriver;
                connect(&inputDriver, &QTimer::timeout, dialog, [&]() {
                    auto *input = qobject_cast<QInputDialog *>(QApplication::activeModalWidget());
                    if (!input) return;
                    if (++inputs == 1) { input->setTextValue("QA feed"); input->accept(); }
                    else { input->setTextValue(url); if (cancel) input->reject(); else input->accept(); }
                });
                inputDriver.start(10);
                add->click();
                inputDriver.stop();
                finalCount = list->count();
                if (finalCount > initialCount) savedUrl = list->item(finalCount - 1)->toolTip();
                if (auto *status = dialog->findChild<QLabel *>("feedSaveStatus")) errorVisible = !status->text().isEmpty() && !status->isHidden();
            }
            dialog->accept();
        });
        QTimer watchdog;
        connect(&watchdog, &QTimer::timeout, &options, []() {
            if (auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget())) dialog->reject();
        });
        watchdog.start(2000);
        manage->click();
        watchdog.stop();
        QCOMPARE(inputs, 2);
        QCOMPARE(initialCount, 3);
        QCOMPARE(finalCount, initialCount + (valid ? 1 : 0));
        QCOMPARE(errorVisible, !valid && !cancel);
        QFile saved(profile.filePath("aero7/gadgets/feeds.json"));
        if (valid) {
            QCOMPARE(savedUrl, url.trimmed());
            QVERIFY(saved.open(QIODevice::ReadOnly));
            const auto entries = QJsonDocument::fromJson(saved.readAll()).array();
            QCOMPARE(entries.size(), 4);
            QCOMPARE(entries.last().toObject().value("url").toString(), savedUrl);
        } else {
            QVERIFY(!saved.exists());
        }
    }

    void feedManagerPreservesChoices_data()
    {
        QTest::addColumn<QString>("scenario");
        QTest::newRow("empty-list-survives-reopen") << QString("empty");
        QTest::newRow("failed-save-preserves-list") << QString("failure");
        QTest::newRow("failed-save-can-retry") << QString("retry");
        QTest::newRow("typed-url-survives-manager") << QString("typed");
    }

    void feedManagerRetainsDamagedCatalog_data()
    {
        QTest::addColumn<QByteArray>("contents");
        QTest::newRow("truncated-json") << QByteArray("[{\"name\":\"My saved feed\"");
        QTest::newRow("wrong-root") << QByteArray("{\"name\":\"My saved feed\",\"url\":\"https://qa.invalid/feed\"}");
        QTest::newRow("invalid-row") << QByteArray("[{\"name\":\"Keep me\",\"url\":\"https://qa.invalid/feed\"},42]");
        QTest::newRow("missing-url") << QByteArray("[{\"name\":\"Keep me\",\"url\":\"https://qa.invalid/feed\"},{\"name\":\"Incomplete\"}]");
        QTest::newRow("invalid-address") << QByteArray("[{\"name\":\"Saved feed\",\"url\":\"https://[broken/feed\"}]");
        QTest::newRow("oversized") << (QByteArray("[]") + QByteArray(1024 * 1024, ' '));
        QTest::newRow("unreadable-file") << QByteArray("[{\"name\":\"Saved feed\",\"url\":\"https://qa.invalid/feed\"}]");
    }
    void feedManagerRetainsDamagedCatalog()
    {
        QFETCH(QByteArray, contents);
        QTemporaryDir profile;
        QVERIFY(profile.isValid());
        const auto oldConfig = qgetenv("XDG_CONFIG_HOME");
        qputenv("XDG_CONFIG_HOME", profile.path().toUtf8());
        const auto restoreConfig = qScopeGuard([&]() { qputenv("XDG_CONFIG_HOME", oldConfig); });
        QVERIFY(QDir().mkpath(profile.filePath("aero7/gadgets")));
        QFile file(profile.filePath("aero7/gadgets/feeds.json"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(contents), contents.size());
        file.close();
        const auto permissions = file.permissions();
        const bool unreadable = QString::fromLatin1(QTest::currentDataTag()) == "unreadable-file";
        const auto restorePermissions = qScopeGuard([&]() { file.setPermissions(permissions); });
        if (unreadable) {
            QVERIFY(file.setPermissions({}));
            if (file.open(QIODevice::ReadOnly)) {
                file.close();
                QSKIP("This user can bypass file read permissions");
            }
        }
        const auto *definition = m_manager->definition("org.aero7.gadgets.feeds");
        QVERIFY(definition);
        GadgetOptionsDialog options(*definition, {});
        QPushButton *manage = nullptr;
        for (auto *button : options.findChildren<QPushButton *>()) if (button->text() == "Manage feeds...") manage = button;
        QVERIFY(manage);
        bool visibleError = false, protectedActions = false, originalRetained = false;
        bool retryRestored = false, retryErrorCleared = false;
        int initialCount = -1;
        QTimer::singleShot(0, &options, [&]() {
            auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            if (!dialog || dialog->windowTitle() != "Manage Feeds") return;
            auto *list = dialog->findChild<QListWidget *>();
            auto *status = dialog->findChild<QLabel *>("feedSaveStatus");
            QPushButton *add = nullptr, *remove = nullptr, *retry = nullptr;
            for (auto *button : dialog->findChildren<QPushButton *>()) {
                if (button->text() == "Add...") add = button;
                if (button->text() == "Remove") remove = button;
                if (button->text() == "Try Again") retry = button;
            }
            if (list && add && remove && status) {
                initialCount = list->count();
                visibleError = !status->isHidden() && status->text().contains("could not", Qt::CaseInsensitive);
                protectedActions = !add->isEnabled() && !remove->isEnabled();
                remove->click();
                if (unreadable) file.setPermissions(permissions);
                if (file.open(QIODevice::ReadOnly)) { originalRetained = file.readAll() == contents; file.close(); }
                const QByteArray repaired = "[{\"name\":\"Recovered feed\",\"url\":\"https://qa.invalid/restored.atom\"}]";
                if (retry && file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                    const bool written = file.write(repaired) == repaired.size();
                    file.close();
                    if (written) {
                        retry->click();
                        retryRestored = list->count() == 1 && list->item(0)->toolTip() == "https://qa.invalid/restored.atom"
                            && add->isEnabled() && remove->isEnabled();
                        retryErrorCleared = status->text().isEmpty() && status->isHidden();
                    }
                }
            }
            dialog->accept();
        });
        QTimer::singleShot(1500, &options, []() {
            if (auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget())) dialog->reject();
        });
        manage->click();
        QVERIFY2(originalRetained, "Manage Feeds must never overwrite an unreadable or damaged catalog with fallback/filtered entries");
        QCOMPARE(initialCount, 0);
        QVERIFY(visibleError);
        QVERIFY(protectedActions);
        QVERIFY(retryRestored);
        QVERIFY(retryErrorCleared);
    }
    void feedManagerPreservesChoices()
    {
        QFETCH(QString, scenario);
        QTemporaryDir profile;
        QVERIFY(profile.isValid());
        const auto oldConfig = qgetenv("XDG_CONFIG_HOME");
        qputenv("XDG_CONFIG_HOME", profile.path().toUtf8());
        const auto restoreConfig = qScopeGuard([&]() { qputenv("XDG_CONFIG_HOME", oldConfig); });
        const auto *definition = m_manager->definition("org.aero7.gadgets.feeds");
        QVERIFY(definition);
        GadgetOptionsDialog options(*definition, {});
        QPushButton *manage = nullptr;
        for (auto *button : options.findChildren<QPushButton *>()) if (button->text() == "Manage feeds...") manage = button;
        QVERIFY(manage);
        if (scenario == "typed") {
            const auto combos = options.findChildren<QComboBox *>();
            QCOMPARE(combos.size(), 1);
            combos.first()->setEditText("https://qa.invalid/new.xml");
        }
        const auto before = options.settings();
        bool observed = false;
        bool errorVisible = false;
        bool retryClearedError = false;
        int retryCount = -1;
        int initialCount = -1, finalCount = -1;
        QTimer::singleShot(0, &options, [&]() {
            auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            if (!dialog || dialog->windowTitle() != "Manage Feeds") return;
            auto *list = dialog->findChild<QListWidget *>();
            QPushButton *remove = nullptr;
            for (auto *button : dialog->findChildren<QPushButton *>()) if (button->text() == "Remove") remove = button;
            if (list && remove) {
                initialCount = list->count();
                if (scenario == "empty") for (int i = 0; i < initialCount; ++i) remove->click();
                if (scenario == "failure" || scenario == "retry") {
                    // Block a save after a successful load; unreadable catalogs
                    // now have their own preservation/reload coverage.
                    QDir().mkpath(profile.filePath("aero7/gadgets/feeds.json"));
                    remove->click();
                }
                finalCount = list->count();
                if (auto *status = dialog->findChild<QLabel *>("feedSaveStatus")) errorVisible = !status->text().isEmpty() && !status->isHidden();
                if (scenario == "retry" && QDir().rmdir(profile.filePath("aero7/gadgets/feeds.json"))) {
                    remove->click();
                    retryCount = list->count();
                    if (auto *status = dialog->findChild<QLabel *>("feedSaveStatus")) retryClearedError = status->text().isEmpty() && status->isHidden();
                }
                observed = true;
            }
            dialog->accept();
        });
        QTimer::singleShot(1000, &options, []() {
            if (auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget())) dialog->reject();
        });
        manage->click();
        QVERIFY(observed);
        QCOMPARE(initialCount, 3);
        if (scenario == "failure" || scenario == "retry") {
            QCOMPARE(finalCount, initialCount);
            QVERIFY(errorVisible);
            if (scenario == "retry") {
                QCOMPARE(retryCount, initialCount - 1);
                QVERIFY(retryClearedError);
                QFile saved(profile.filePath("aero7/gadgets/feeds.json"));
                QVERIFY(saved.open(QIODevice::ReadOnly));
                QCOMPARE(QJsonDocument::fromJson(saved.readAll()).array().size(), retryCount);
            }
        } else if (scenario == "typed") {
            QCOMPARE(options.settings(), before);
        } else {
            QCOMPARE(finalCount, 0);
            QFile saved(profile.filePath("aero7/gadgets/feeds.json"));
            QVERIFY(saved.open(QIODevice::ReadOnly));
            QCOMPARE(QJsonDocument::fromJson(saved.readAll()).array().size(), 0);
            int reopenedCount = -1;
            QTimer::singleShot(0, &options, [&]() {
                auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
                if (!dialog || dialog->windowTitle() != "Manage Feeds") return;
                if (auto *list = dialog->findChild<QListWidget *>()) reopenedCount = list->count();
                dialog->accept();
            });
            manage->click();
            QCOMPARE(reopenedCount, 0);
        }
    }

    void weatherFindRejectsInvalidReplies_data()
    {
        QTest::addColumn<QByteArray>("payload");
        QTest::addColumn<bool>("networkError");
        QTest::newRow("http-error-with-result") << QByteArray(R"({"results":[{"name":"Wrong city","latitude":1,"longitude":2}]})") << true;
        QTest::newRow("missing-coordinates") << QByteArray(R"({"results":[{"name":"Wrong city"}]})") << false;
        QTest::newRow("string-coordinates") << QByteArray(R"({"results":[{"name":"Wrong city","latitude":"12","longitude":"34"}]})") << false;
        QTest::newRow("out-of-range") << QByteArray(R"({"results":[{"name":"Wrong city","latitude":91,"longitude":181}]})") << false;
        QTest::newRow("missing-name") << QByteArray(R"({"results":[{"latitude":12,"longitude":34}]})") << false;
        QTest::newRow("empty-results") << QByteArray(R"({"results":[]})") << false;
        QTest::newRow("invalid-json") << QByteArray("not json") << false;
        QTest::newRow("oversized") << (QByteArray(R"({"results":[{"name":"Wrong city","latitude":1,"longitude":2}]})") + QByteArray(1024 * 1024, ' ')) << false;
    }
    void weatherLookupMessageFits_data()
    {
        QTest::addColumn<int>("pixels");
        QTest::newRow("small-font") << 12;
        QTest::newRow("normal-font") << 16;
        QTest::newRow("large-font") << 24;
    }
    void weatherLookupMessageFits()
    {
        QFETCH(int, pixels);
        const auto *definition = m_manager->definition("org.aero7.gadgets.weather");
        QVERIFY(definition);
        FixtureNetwork network;
        GadgetOptionsDialog dialog(*definition, {{"location", "QA City"}}, nullptr, &network);
        auto font = dialog.font(); font.setPixelSize(pixels); dialog.setFont(font);
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));
        dialog.resize(dialog.minimumWidth(), dialog.sizeHint().height());
        QCoreApplication::processEvents();
        QPushButton *find = nullptr;
        for (auto *button : dialog.findChildren<QPushButton *>()) if (button->text() == "Find") find = button;
        QVERIFY(find);
        find->click();
        QCOMPARE(network.replies.size(), 1);
        network.replies.last()->finishBytes({}, QNetworkReply::ConnectionRefusedError);
        QCoreApplication::processEvents();
        auto *status = dialog.findChild<QLabel *>("locationLookupStatus");
        QVERIFY(status && status->isVisible());
        QVERIFY2(status->height() >= status->heightForWidth(status->width()),
                 qPrintable(QString("label height %1, needed %2 at width %3").arg(status->height()).arg(status->heightForWidth(status->width())).arg(status->width())));
        QVERIFY(dialog.rect().contains(status->geometry()));
    }
    void weatherFindRejectsInvalidReplies()
    {
        QFETCH(QByteArray, payload);
        QFETCH(bool, networkError);
        const auto *definition = m_manager->definition("org.aero7.gadgets.weather");
        QVERIFY(definition);
        FixtureNetwork network;
        GadgetOptionsDialog dialog(*definition, {{"location", "QA City"}, {"latitude", 51.965}, {"longitude", 6.288}}, nullptr, &network);
        const auto before = dialog.settings();
        QPushButton *find = nullptr;
        for (auto *button : dialog.findChildren<QPushButton *>()) if (button->text() == "Find") find = button;
        QVERIFY(find);
        find->click();
        QCOMPARE(network.replies.size(), 1);
        QVERIFY(!find->isEnabled());
        network.replies.last()->finishBytes(payload, networkError ? QNetworkReply::ContentAccessDenied : QNetworkReply::NoError);
        QCOMPARE(dialog.settings(), before);
        QVERIFY(find->isEnabled());
        auto *status = dialog.findChild<QLabel *>("locationLookupStatus");
        QVERIFY(status);
        QVERIFY(!status->text().isEmpty());
    }
    void weatherFindPreservesEdits_data()
    {
        QTest::addColumn<QString>("edit");
        QTest::newRow("location") << QString("location");
        QTest::newRow("location-away-and-back") << QString("roundtrip");
        QTest::newRow("coordinates") << QString("coordinates");
    }
    void weatherFindPreservesEdits()
    {
        QFETCH(QString, edit);
        const auto *definition = m_manager->definition("org.aero7.gadgets.weather");
        QVERIFY(definition);
        FixtureNetwork network;
        GadgetOptionsDialog dialog(*definition, {{"location", "QA City"}, {"latitude", 51.965}, {"longitude", 6.288}}, nullptr, &network);
        QPushButton *find = nullptr;
        for (auto *button : dialog.findChildren<QPushButton *>()) if (button->text() == "Find") find = button;
        QVERIFY(find);
        find->click();
        QCOMPARE(network.replies.size(), 1);
        if (edit == "coordinates") {
            const auto spins = dialog.findChildren<QDoubleSpinBox *>();
            QVERIFY(!spins.isEmpty());
            spins.first()->setValue(-12);
        } else {
            QLineEdit *location = nullptr;
            for (auto *line : dialog.findChildren<QLineEdit *>()) if (line->text() == "QA City") location = line;
            QVERIFY(location);
            location->setText("Different city");
            if (edit == "roundtrip") location->setText("QA City");
        }
        const auto edited = dialog.settings();
        network.replies.last()->finishBytes(R"({"results":[{"name":"Old result","latitude":12,"longitude":34}]})");
        QCOMPARE(dialog.settings(), edited);
        QVERIFY(find->isEnabled());
    }

    void weatherFindValidResultAndRetry_data()
    {
        QTest::addColumn<double>("latitude");
        QTest::addColumn<double>("longitude");
        QTest::newRow("zero") << 0.0 << 0.0;
        QTest::newRow("southern") << -33.8688 << 151.2093;
        QTest::newRow("lower-bound") << -90.0 << -180.0;
        QTest::newRow("upper-bound") << 90.0 << 180.0;
    }
    void weatherFindValidResultAndRetry()
    {
        QFETCH(double, latitude);
        QFETCH(double, longitude);
        const auto *definition = m_manager->definition("org.aero7.gadgets.weather");
        QVERIFY(definition);
        FixtureNetwork network;
        GadgetOptionsDialog dialog(*definition, {{"location", "QA City"}, {"latitude", 51.965}, {"longitude", 6.288}}, nullptr, &network);
        QPushButton *find = nullptr;
        for (auto *button : dialog.findChildren<QPushButton *>()) if (button->text() == "Find") find = button;
        QVERIFY(find);
        find->click();
        QCOMPARE(network.replies.size(), 1);
        network.replies.last()->finishBytes({}, QNetworkReply::TimeoutError);
        QVERIFY(find->isEnabled());
        find->click();
        QCOMPARE(network.replies.size(), 2);
        network.replies.last()->finish(QJsonObject{{"results", QJsonArray{QJsonObject{{"name", "Found City"}, {"country", "QA Country"}, {"latitude", latitude}, {"longitude", longitude}}}}});
        const auto saved = dialog.settings();
        QCOMPARE(saved.value("location").toString(), QString("Found City, QA Country"));
        QCOMPARE(saved.value("latitude").toDouble(), latitude);
        QCOMPARE(saved.value("longitude").toDouble(), longitude);
        QVERIFY(find->isEnabled());
        auto *status = dialog.findChild<QLabel *>("locationLookupStatus");
        QVERIFY(status);
        QVERIFY(status->text().isEmpty());
    }
    void weatherFindEmptyQueryDoesNotRequest()
    {
        const auto *definition = m_manager->definition("org.aero7.gadgets.weather");
        QVERIFY(definition);
        FixtureNetwork network;
        GadgetOptionsDialog dialog(*definition, {{"location", "  "}}, nullptr, &network);
        QPushButton *find = nullptr;
        for (auto *button : dialog.findChildren<QPushButton *>()) if (button->text() == "Find") find = button;
        QVERIFY(find);
        find->click();
        QVERIFY(network.replies.isEmpty());
        auto *status = dialog.findChild<QLabel *>("locationLookupStatus");
        QVERIFY(status);
        QVERIFY(!status->text().isEmpty());
    }
    void weatherFindDestroyedDialogIgnoresReply()
    {
        const auto *definition = m_manager->definition("org.aero7.gadgets.weather");
        QVERIFY(definition);
        FixtureNetwork network;
        {
            GadgetOptionsDialog dialog(*definition, {{"location", "QA City"}}, nullptr, &network);
            QPushButton *find = nullptr;
            for (auto *button : dialog.findChildren<QPushButton *>()) if (button->text() == "Find") find = button;
            QVERIFY(find);
            find->click();
            QCOMPARE(network.replies.size(), 1);
        }
        network.replies.last()->finishBytes(R"({"results":[{"name":"Old result","latitude":12,"longitude":34}]})");
        QCoreApplication::processEvents();
    }


    void allGadgetOptionsRoundTrip_data()
    {
        QTest::addColumn<QString>("id");
        QTest::addColumn<QJsonObject>("settings");
        QTest::newRow("clock") << QString("clock") << QJsonObject{{"face", 2}, {"label", "QA Clock"}, {"timezone", "UTC"}, {"seconds", false}};
        QTest::newRow("calendar") << QString("calendar") << QJsonObject{{"firstDay", 7}, {"highlightToday", false}, {"weekNumbers", true}};
        QTest::newRow("cpu") << QString("cpu") << QJsonObject{};
        QTest::newRow("currency") << QString("currency") << QJsonObject{{"base", "GBP"}, {"target", "JPY"}, {"amount", 123.45}};
        QTest::newRow("feeds") << QString("feeds") << QJsonObject{{"feed", "https://qa.invalid/custom.atom"}, {"refreshMinutes", 60}, {"count", 12}, {"openLinks", false}};
        QTest::newRow("puzzle") << QString("picturepuzzle") << QJsonObject{{"image", "custom"}, {"customImage", "/tmp/qa-picture.png"}, {"difficulty", 5}};
        QTest::newRow("slideshow") << QString("slideshow") << QJsonObject{{"folder", "/tmp/qa-pictures"}, {"delaySeconds", 90}, {"transition", "none"}, {"shuffle", true}};
        QTest::newRow("weather") << QString("weather") << QJsonObject{{"location", "QA City"}, {"latitude", -33.8688}, {"longitude", 151.2093}, {"unit", "fahrenheit"}, {"refreshMinutes", 60}};
        QTest::newRow("media") << QString("mediacenter") << QJsonObject{{"player", "auto"}};
    }
    void allGadgetOptionsRoundTrip()
    {
        QFETCH(QString, id);
        QFETCH(QJsonObject, settings);
        const auto *definition = m_manager->definition("org.aero7.gadgets." + id);
        QVERIFY(definition);
        settings.insert("qaPreservedExtension", "unchanged");
        GadgetOptionsDialog dialog(*definition, settings);
        QCOMPARE(dialog.settings(), settings);
        GadgetOptionsDialog reopened(*definition, dialog.settings());
        QCOMPARE(reopened.settings(), settings);
    }
    void acceptedRefreshOptionsUpdateTimer_data()
    {
        QTest::addColumn<QString>("id");
        QTest::addColumn<bool>("accept");
        for (const QString id : {QString("weather"), QString("feeds")}) {
            QTest::newRow(qPrintable(id + "-accept")) << id << true;
            QTest::newRow(qPrintable(id + "-cancel")) << id << false;
        }
    }
    void storedRefreshIntervalsAreBounded_data()
    {
        QTest::addColumn<QString>("id");
        QTest::addColumn<int>("stored");
        QTest::addColumn<int>("expected");
        QTest::newRow("weather-min") << QString("weather") << -1 << 15;
        QTest::newRow("weather-overflow") << QString("weather") << std::numeric_limits<int>::max() << 360;
        QTest::newRow("feeds-min") << QString("feeds") << -1 << 5;
        QTest::newRow("feeds-overflow") << QString("feeds") << std::numeric_limits<int>::max() << 1440;
    }
    void storedRefreshIntervalsAreBounded()
    {
        QFETCH(QString, id);
        QFETCH(int, stored);
        QFETCH(int, expected);
        const auto *definition = m_manager->definition("org.aero7.gadgets." + id);
        QVERIFY(definition);
        GadgetState state;
        state.settings = {{"refreshMinutes", stored}};
        GadgetWindow window(*definition, state, m_manager.get());
        QCOMPARE(window.m_networkTimer.interval(), expected * 60 * 1000);
        QVERIFY(window.m_networkTimer.isActive());
    }
    void acceptedRefreshOptionsUpdateTimer()
    {
        QFETCH(QString, id);
        QFETCH(bool, accept);
        auto *services = RuntimeServices::instance();
        FixtureNetwork network;
        auto *previous = services->m_network;
        services->m_network = &network;
        const auto restoreNetwork = qScopeGuard([&]() { services->m_network = previous; });
        const auto *definition = m_manager->definition("org.aero7.gadgets." + id);
        QVERIFY(definition);
        GadgetState state;
        state.settings = {{"refreshMinutes", 30}};
        GadgetWindow window(*definition, state, m_manager.get());
        QCOMPARE(window.m_networkTimer.interval(), 30 * 60 * 1000);
        bool observed = false;
        QTimer::singleShot(0, &window, [&]() {
            auto *dialog = qobject_cast<GadgetOptionsDialog *>(QApplication::activeModalWidget());
            if (!dialog) return;
            for (auto *spin : dialog->findChildren<QSpinBox *>()) {
                if (spin->maximum() == (id == "weather" ? 360 : 1440)) {
                    spin->setValue(60);
                    observed = true;
                }
            }
            if (accept) dialog->accept(); else dialog->reject();
        });
        QTimer::singleShot(1000, &window, []() {
            if (auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget())) dialog->reject();
        });
        window.showOptions();
        QVERIFY(observed);
        QCOMPARE(window.m_state.settings.value("refreshMinutes").toInt(), accept ? 60 : 30);
        QCOMPARE(window.m_networkTimer.interval(), (accept ? 60 : 30) * 60 * 1000);
        QVERIFY(window.m_networkTimer.isActive());
    }
    void puzzleOptionsPreserveCurrentBoard_data()
    {
        QTest::addColumn<bool>("accept");
        QTest::newRow("accept-unchanged") << true;
        QTest::newRow("cancel") << false;
    }
    void puzzleOptionsPreserveCurrentBoard()
    {
        QFETCH(bool, accept);
        const auto *definition = m_manager->definition("org.aero7.gadgets.picturepuzzle");
        QVERIFY(definition);
        GadgetState state;
        state.settings = {{"difficulty", 3}, {"image", "aero7-flower"}, {"customImage", ""}};
        GadgetWindow window(*definition, state, m_manager.get());
        window.m_data.puzzleTiles = {0, 1, 2, 3, 4, 5, 6, 8, 7};
        window.m_data.puzzleMoves = 42;
        const auto board = window.m_data.puzzleTiles;
        bool observed = false;
        QTimer::singleShot(0, &window, [&]() {
            auto *dialog = qobject_cast<GadgetOptionsDialog *>(QApplication::activeModalWidget());
            if (!dialog) return;
            observed = true;
            if (accept) dialog->accept(); else dialog->reject();
        });
        QTimer::singleShot(1000, &window, []() {
            if (auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget())) dialog->reject();
        });
        window.showOptions();
        QVERIFY(observed);
        QCOMPARE(window.m_state.settings, state.settings);
        QCOMPARE(window.m_data.puzzleTiles, board);
        QCOMPARE(window.m_data.puzzleMoves, 42);
    }

    void puzzleOptionsNewGame_data()
    {
        QTest::addColumn<QString>("action");
        QTest::addColumn<bool>("accept");
        for (const QString action : {QString("new"), QString("difficulty"), QString("image"), QString("custom")}) {
            QTest::newRow(qPrintable(action + "-accept")) << action << true;
            if (action != "new") QTest::newRow(qPrintable(action + "-cancel")) << action << false;
        }
        QTest::newRow("default-fields") << QString("defaults") << true;
        QTest::newRow("enter-unchanged") << QString("enter") << true;
    }
    void puzzleOptionsNewGame()
    {
        QFETCH(QString, action);
        QFETCH(bool, accept);
        const auto *definition = m_manager->definition("org.aero7.gadgets.picturepuzzle");
        QVERIFY(definition);
        QTemporaryDir images;
        QVERIFY(images.isValid());
        QImage custom(8, 8, QImage::Format_RGB32);
        custom.fill(Qt::red);
        const QString customPath = images.filePath("custom.png");
        QVERIFY(custom.save(customPath));
        GadgetState state;
        if (action != "defaults") state.settings = {{"difficulty", 3}, {"image", action == "custom" ? "custom" : "aero7-flower"}};
        GadgetWindow window(*definition, state, m_manager.get());
        const auto board = window.m_data.puzzleTiles;
        const auto originalImage = window.m_data.puzzleImage;
        window.m_data.puzzleMoves = 42;
        bool observed = false;
        QTimer::singleShot(0, &window, [&]() {
            auto *dialog = qobject_cast<GadgetOptionsDialog *>(QApplication::activeModalWidget());
            if (!dialog) return;
            if (action == "new") {
                for (auto *button : dialog->findChildren<QPushButton *>()) {
                    if (button->text() == "New puzzle") {
                        observed = true;
                        button->click();
                        return;
                    }
                }
            } else if (action == "difficulty" || action == "image") {
                const QString target = action == "difficulty" ? "5" : "aero7-aurora";
                for (auto *combo : dialog->findChildren<QComboBox *>()) {
                    const int index = combo->findText(target);
                    if (index >= 0) { combo->setCurrentIndex(index); observed = true; }
                }
            } else if (action == "custom") {
                const auto lines = dialog->findChildren<QLineEdit *>();
                if (lines.size() == 1) { lines.first()->setText(customPath); observed = true; }
            } else if (action == "enter") {
                observed = true;
                const auto combos = dialog->findChildren<QComboBox *>();
                if (!combos.isEmpty()) {
                    combos.first()->setFocus();
                    QTest::keyClick(combos.first(), Qt::Key_Return);
                    return;
                }
            } else observed = true;
            if (accept) dialog->accept(); else dialog->reject();
        });
        QTimer::singleShot(1000, &window, []() {
            if (auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget())) dialog->reject();
        });
        window.showOptions();
        QVERIFY(observed);
        if (!accept || action == "defaults" || action == "enter") {
            if (accept) QVERIFY(window.m_state.settings.contains("customImage"));
            QCOMPARE(window.m_data.puzzleTiles, board);
            QCOMPARE(window.m_data.puzzleImage, originalImage);
            QCOMPARE(window.m_data.puzzleMoves, 42);
            if (!accept) QCOMPARE(window.m_state.settings, state.settings);
        } else {
            QCOMPARE(window.m_data.puzzleMoves, 0);
            const int side = action == "difficulty" ? 5 : 3;
            auto tiles = window.m_data.puzzleTiles;
            QCOMPARE(tiles.size(), side * side);
            std::sort(tiles.begin(), tiles.end());
            for (int i = 0; i < tiles.size(); ++i) QCOMPARE(tiles[i], i);
            if (action == "custom") QCOMPARE(window.m_data.puzzleImage.pixelColor(0, 0), QColor(Qt::red));
            if (action == "image") QVERIFY(window.m_data.puzzleImage != originalImage);
        }
    }

    void mediaVisibleControlsRouteCommands_data()
    {
        QTest::addColumn<QString>("size");
        QTest::addColumn<int>("control");
        for (const QString size : {QString("small"), QString("large")}) {
            for (int control = 0; control < 4; ++control) {
                QTest::newRow(qPrintable(size + QString::number(control))) << size << control;
            }
        }
    }
    void mediaVisibleControlsRouteCommands()
    {
        QFETCH(QString, size);
        QFETCH(int, control);
        QTemporaryDir profile;
        QVERIFY(profile.isValid());
        const QString statePath = profile.filePath("player.json");
        QFile stateFile(statePath);
        QVERIFY(stateFile.open(QIODevice::WriteOnly));
        stateFile.write("{\"art\":\"\",\"title\":\"QA Track\",\"status\":\"Playing\"}");
        stateFile.close();
        QProcess player;
        const auto stopPlayer = qScopeGuard([&]() {
            player.terminate();
            if (!player.waitForFinished(2000)) { player.kill(); player.waitForFinished(); }
        });
        player.start(QCoreApplication::applicationDirPath() + "/gadget-media-services-test",
                     {"--fixture-player", statePath});
        QVERIFY(player.waitForStarted());
        QVERIFY(player.waitForReadyRead());
        QCOMPARE(player.readLine(), QByteArray("READY\n"));
        auto *services = RuntimeServices::instance();
        services->updateMedia();
        const auto *definition = m_manager->definition("org.aero7.gadgets.mediacenter");
        QVERIFY(definition);
        GadgetState state;
        state.size = size;
        GadgetWindow window(*definition, state, m_manager.get());
        const QRectF panel = QRectF(window.bodyRect()).adjusted(2, 2, -2, -2);
        const qreal x = panel.center().x() + (control - 1) * 42;
        const QPoint point = control == 3
            ? QPoint(15, window.bodyRect().height() - 10)
            : QPoint(qRound(x), qRound(panel.top() + panel.height() * .77));
        QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, point);
        const auto commands = [&]() {
            QFile file(statePath + ".commands");
            return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
        };
        if (control == 3) {
            QTest::qWait(100);
            QVERIFY2(commands().isEmpty(), "Clicking blank footer space sent a player command");
        } else {
            const QByteArray expected[] = {"Previous\n", "PlayPause\n", "Next\n"};
            QTRY_COMPARE_WITH_TIMEOUT(commands(), expected[control], 1000);
        }
    }
    void mediaCoverKeepsTextReadable_data()
    {
        QTest::addColumn<QSize>("dimensions");
        QTest::newRow("small") << QSize(232, 166);
        QTest::newRow("large") << QSize(310, 230);
    }
    void mediaCoverKeepsTextReadable()
    {
        QFETCH(QSize, dimensions);
        const auto *definition = m_manager->definition("org.aero7.gadgets.mediacenter");
        QVERIFY(definition);
        GadgetRenderData data;
        data.mediaAvailable = true;
        data.mediaArtist = "MMMMMMMM";
        data.mediaArt = QImage(8, 8, QImage::Format_ARGB32);
        data.mediaArt.fill(Qt::red);
        QImage image(dimensions, QImage::Format_ARGB32);
        image.fill(Qt::black);
        QPainter painter(&image);
        GadgetPainter::paint(painter, *definition, GadgetState(), data, image.rect());
        painter.end();
        const QRectF panel = QRectF(image.rect()).adjusted(2, 2, -2, -2);
        const int top = int(panel.top() + panel.height() * .64);
        int brightestText = 0;
        for (int y = top + 3; y < top + 14; ++y) {
            for (int x = 69; x < qMin(dimensions.width() - 20, 180); ++x) {
                const auto color = image.pixelColor(x, y);
                brightestText = qMax(brightestText, qMin(color.red(), qMin(color.green(), color.blue())));
            }
        }
        QVERIFY2(brightestText > 220, "Album-art border opacity leaked into the white track text");
    }

    void opacityComposesFinalSurface_data()
    {
        QTest::addColumn<QString>("id");
        QTest::addColumn<int>("percent");
        QTest::addColumn<bool>("restore");
        for (int percent : {20, 40, 60, 80}) {
            const QByteArray name = QByteArray("calendar-") + QByteArray::number(percent);
            QTest::newRow(name.constData()) << QStringLiteral("calendar") << percent << false;
        }
        QTest::newRow("calendar-restored") << QStringLiteral("calendar") << 60 << true;
        QTest::newRow("slideshow-crossfade") << QStringLiteral("slideshow") << 40 << false;
        QTest::newRow("slideshow-restored") << QStringLiteral("slideshow") << 80 << true;
    }

    void opacityComposesFinalSurface()
    {
        QFETCH(QString, id);
        QFETCH(int, percent);
        QFETCH(bool, restore);
        const auto *definition = m_manager->definition("org.aero7.gadgets." + id);
        QVERIFY(definition);
        GadgetState state;
        state.opacity = restore ? percent : 100;
        GadgetWindow window(*definition, state, m_manager.get());
        window.m_repaintTimer.stop();
        window.m_slideTimer.stop();
        window.m_slideAnimation.stop();
        window.m_controlsAnimation.stop();
        qInfo("OPACITY_DPR=%g", window.devicePixelRatioF());
        window.m_controlsOpacity = 0.7;
        if (id == "slideshow") {
            window.m_data.previousSlideImage = QImage(12, 8, QImage::Format_RGB32);
            window.m_data.previousSlideImage.fill(Qt::red);
            window.m_data.slideImage = QImage(12, 8, QImage::Format_RGB32);
            window.m_data.slideImage.fill(Qt::blue);
            window.m_data.slideTransition = 0.35;
        }
        const auto capture = [&]() {
            return window.grab().toImage().convertToFormat(QImage::Format_ARGB32_Premultiplied);
        };
        const QImage initial = capture();
        window.setOpacityPercent(100);
        const QImage opaque = capture();
        window.setOpacityPercent(percent);
        const QImage translucent = capture();
        if (restore) QCOMPARE(initial, translucent);

        // Compose the already-finished body, text, crossfade and hover controls
        // as one surface. Per-primitive opacity would incorrectly accumulate
        // alpha where those elements overlap or reset their local opacity.
        QImage expected(opaque.size(), opaque.format());
        expected.setDevicePixelRatio(opaque.devicePixelRatio());
        expected.fill(Qt::transparent);
        {
            QPainter painter(&expected);
            painter.setOpacity(percent / 100.0);
            painter.drawImage(QPoint(0, 0), opaque);
        }
        QVERIFY2(translucent != opaque, "Opacity must change surface pixels, not only a window property");
        QCOMPARE(translucent.size(), expected.size());
        int maxError = 0;
        for (int y = 0; y < expected.height(); ++y) {
            const auto *want = reinterpret_cast<const QRgb *>(expected.constScanLine(y));
            const auto *got = reinterpret_cast<const QRgb *>(translucent.constScanLine(y));
            for (int x = 0; x < expected.width(); ++x) {
                for (int shift : {0, 8, 16, 24}) {
                    maxError = std::max(maxError, std::abs(int((want[x] >> shift) & 255) - int((got[x] >> shift) & 255)));
                }
            }
        }
        QVERIFY2(maxError <= 2, qPrintable(QStringLiteral("Composed opacity pixel error: %1").arg(maxError)));
        window.setOpacityPercent(100);
        QCOMPARE(capture(), opaque);
    }

    void opacityChangeRequestsRepaint()
    {
        const auto *definition = m_manager->definition("org.aero7.gadgets.calendar");
        QVERIFY(definition);
        GadgetWindow window(*definition, {}, m_manager.get());
        window.m_repaintTimer.stop();
        window.show();
        QTest::qWait(50);
        window.m_controlsAnimation.stop();
        QCoreApplication::sendPostedEvents(&window, QEvent::UpdateRequest);
        UpdateCounter counter;
        window.installEventFilter(&counter);
        window.setOpacityPercent(60);
        QTest::qWait(50);
        QVERIFY2(counter.requests > 0, "Opacity must repaint without waiting for Calendar's timer");
    }

    void dragPreviewPreservesComposedOpacity()
    {
        const auto *definition = m_manager->definition("org.aero7.gadgets.calendar");
        QVERIFY(definition);
        GadgetState state;
        state.opacity = 40;
        GadgetWindow window(*definition, state, m_manager.get());
        window.m_repaintTimer.stop();
        window.m_controlsAnimation.stop();
        const QImage before = window.grab().toImage();
        window.beginLayerDrag();
        QVERIFY(window.m_dragPreview);
        const auto *preview = qobject_cast<QLabel *>(window.m_dragPreview.get());
        QVERIFY(preview);
        QCOMPARE(preview->pixmap().toImage(), before);
        // The preview already contains the alpha; a window-opacity multiplier
        // would apply it twice on other platforms and is unsupported on Wayland.
        QCOMPARE(window.m_dragPreview->windowOpacity(), 1.0);
        window.m_dragPreview.reset();
    }

    void contextMenuKeyboardSelection_data()
    {
        QTest::addColumn<QString>("submenu");
        QTest::addColumn<QString>("choice");
        QTest::newRow("size-large") << QStringLiteral("Size") << QStringLiteral("Large");
        QTest::newRow("opacity-sixty") << QStringLiteral("Opacity") << QStringLiteral("60%");
    }

    void contextMenuKeyboardSelection()
    {
        QFETCH(QString, submenu);
        QFETCH(QString, choice);
        const auto *definition = m_manager->definition("org.aero7.gadgets.calendar");
        QVERIFY(definition);
        GadgetWindow window(*definition, {}, m_manager.get());
        window.m_repaintTimer.stop();
        QString failure;
        bool selected = false;
        QTimer watchdog;
        watchdog.setSingleShot(true);
        connect(&watchdog, &QTimer::timeout, &window, [&]() {
            failure = QStringLiteral("Keyboard menu selection timed out");
            for (int count = 0; count < 8 && QApplication::activePopupWidget(); ++count)
                QApplication::activePopupWidget()->close();
        });
        watchdog.start(2000);
        QTimer::singleShot(0, &window, [&]() {
            auto *root = qobject_cast<QMenu *>(QApplication::activePopupWidget());
            if (!root) return;
            QAction *branch = nullptr;
            for (QAction *action : root->actions()) {
                if (action->text() == submenu) branch = action;
            }
            if (!branch || !branch->menu()) {
                failure = QStringLiteral("Keyboard submenu missing");
                root->close();
                return;
            }
            root->setActiveAction(branch);
            QTest::keyClick(root, Qt::Key_Right);
            QMenu *menu = branch->menu();
            menu->setFocus(Qt::OtherFocusReason);
            if (!menu->isVisible() || !menu->hasFocus()) {
                failure = QStringLiteral("Submenu did not acquire keyboard focus");
                root->close();
                return;
            }
            for (int count = 0; count <= menu->actions().size(); ++count) {
                if (menu->activeAction() && menu->activeAction()->text() == choice) {
                    selected = true;
                    QTest::keyClick(menu, Qt::Key_Return);
                    return;
                }
                QTest::keyClick(menu, Qt::Key_Down);
            }
            failure = QStringLiteral("Keyboard choice not reached");
            root->close();
        });
        // Destruction of the focused submenu is part of the regression:
        // its menu-local style must remain alive while focus is cleared.
        window.showContextMenu(QPoint(50, 50));
        watchdog.stop();
        QVERIFY2(failure.isEmpty(), qPrintable(failure));
        QVERIFY(selected);
        if (submenu == "Size") QCOMPARE(window.stateForSave().size, QStringLiteral("large"));
        else QCOMPARE(window.stateForSave().opacity, 60);
    }

    void contextMenuChecksAreVisible_data()
    {
        QTest::addColumn<QString>("submenu");
        QTest::addColumn<QString>("label");
        QTest::newRow("always-on-top") << QString() << QStringLiteral("Always on top");
        QTest::newRow("size-small") << QStringLiteral("Size") << QStringLiteral("Small");
        QTest::newRow("opacity-full") << QStringLiteral("Opacity") << QStringLiteral("100%");
    }

    void contextMenuChecksAreVisible()
    {
        qInfo("MENU_CHECK_STYLE=%s", qPrintable(QApplication::style()->objectName()));
        QFETCH(QString, submenu);
        QFETCH(QString, label);
        const auto *definition = m_manager->definition("org.aero7.gadgets.calendar");
        QVERIFY(definition);
        GadgetState state;
        GadgetWindow window(*definition, state, m_manager.get());
        window.m_repaintTimer.stop();
        QString failure;
        QImage unchecked, checked, restored;
        bool inspected = false;
        // Operate on the real context menu inside its nested event loop. A
        // watchdog prevents a failed popup discovery from hanging the suite.
        QTimer watchdog;
        watchdog.setSingleShot(true);
        connect(&watchdog, &QTimer::timeout, &window, [&]() {
            failure = QStringLiteral("Context menu inspection timed out");
            if (auto *popup = QApplication::activePopupWidget()) popup->close();
        });
        watchdog.start(2000);
        QTimer::singleShot(0, &window, [&]() {
            auto *root = qobject_cast<QMenu *>(QApplication::activePopupWidget());
            if (!root) return;
            auto inspect = [&]() {
                QMenu *menu = root;
                if (!submenu.isEmpty()) {
                    menu = nullptr;
                    for (QAction *action : root->actions()) {
                        if (action->text() == submenu) menu = action->menu();
                    }
                }
                if (!menu) { failure = QStringLiteral("Submenu missing"); return; }
                QAction *check = nullptr;
                for (QAction *action : menu->actions()) {
                    if (action->text() == label) check = action;
                }
                if (!check || !check->isCheckable()) {
                    failure = QStringLiteral("Checkable action missing"); return;
                }
                menu->ensurePolished();
                menu->adjustSize();
                menu->setActiveAction(nullptr);
                const auto snapshot = [&]() {
                    QImage image(menu->size(), QImage::Format_ARGB32_Premultiplied);
                    image.fill(Qt::transparent);
                    menu->render(&image);
                    // Only compare the indicator gutter, excluding label text.
                    QRect gutter = menu->actionGeometry(check);
                    gutter.setWidth(35);
                    return image.copy(gutter);
                };
                check->setChecked(false);
                unchecked = snapshot();
                check->setChecked(true);
                checked = snapshot();
                check->setChecked(false);
                restored = snapshot();
                inspected = true;
            };
            inspect();
            root->close();
        });
        window.showContextMenu(QPoint(50, 50));
        watchdog.stop();
        QVERIFY2(failure.isEmpty(), qPrintable(failure));
        QVERIFY(inspected);
        QVERIFY(!unchecked.isNull());
        QVERIFY2(unchecked != checked, "Checked state must visibly change the menu indicator gutter");
        QCOMPARE(restored, unchecked);
    }

    void layerChangesRequestImmediateUpdate_data()
    {
        QTest::addColumn<bool>("showDesktopSignal");
        QTest::addColumn<bool>("top");
        QTest::newRow("show-desktop-on") << true << true;
        QTest::newRow("show-desktop-off") << true << false;
        QTest::newRow("always-on-top-on") << false << true;
        QTest::newRow("always-on-top-off") << false << false;
    }

    void layerChangesRequestImmediateUpdate()
    {
        QFETCH(bool, showDesktopSignal);
        QFETCH(bool, top);
        const auto *definition = m_manager->definition("org.aero7.gadgets.calendar");
        QVERIFY(definition);
        GadgetState state;
        GadgetWindow window(*definition, state, m_manager.get());
        window.m_repaintTimer.stop();
        window.winId();
        window.m_layerShell = LayerShellQt::Window::get(window.windowHandle());
        QVERIFY(window.m_layerShell);
        window.m_layerShell->setLayer(top ? LayerShellQt::Window::LayerBottom
                                          : LayerShellQt::Window::LayerTop);
        window.show();
        QTest::qWait(50);
        // Initial hover/controls animation can independently request paints.
        // Drain it before measuring the layer operation itself.
        window.m_controlsAnimation.stop();
        window.m_slideAnimation.stop();
        QCoreApplication::sendPostedEvents(&window, QEvent::UpdateRequest);
        UpdateCounter counter;
        window.installEventFilter(&counter);
        if (showDesktopSignal) {
            QVERIFY(QMetaObject::invokeMethod(KWindowSystem::self(), "showingDesktopChanged",
                                             Qt::DirectConnection, Q_ARG(bool, top)));
        } else {
            window.setAlwaysOnTop(top);
        }
        QCOMPARE(window.m_layerShell->layer(), top ? LayerShellQt::Window::LayerTop
                                                   : LayerShellQt::Window::LayerBottom);
        QTest::qWait(50);
        QVERIFY2(counter.requests > 0,
                 "A layer change must schedule a surface update without waiting for the gadget refresh timer");
    }

    void slideshowOptionsPreservePlayback_data()
    {
        QTest::addColumn<QString>("action");
        QTest::addColumn<bool>("paused");
        for (const QString action : {QString("unchanged"), QString("delay"), QString("shuffle"),
                                     QString("transition"), QString("transition-active"), QString("cancel"), QString("same-folder")}) {
            for (bool paused : {false, true})
                QTest::newRow(qPrintable(action + (paused ? "-paused" : "-playing"))) << action << paused;
        }
    }

    void slideshowOptionsPreservePlayback()
    {
        QFETCH(QString, action);
        QFETCH(bool, paused);
        QTemporaryDir pictures;
        QVERIFY(pictures.isValid());
        QImage image(12, 8, QImage::Format_RGB32);
        image.fill(Qt::red);
        QVERIFY(image.save(pictures.filePath("a.png")));
        image.fill(Qt::blue);
        QVERIFY(image.save(pictures.filePath("b.png")));
        const auto *definition = m_manager->definition("org.aero7.gadgets.slideshow");
        QVERIFY(definition);
        GadgetState state;
        state.settings = {{"folder", pictures.path()}, {"delaySeconds", 600},
                          {"transition", "fade"}, {"shuffle", false}};
        GadgetWindow window(*definition, state, m_manager.get());
        window.nextSlide();
        if (action != "transition-active")
            window.m_slideAnimation.setCurrentTime(window.m_slideAnimation.duration());
        QCOMPARE(window.m_data.slideImage, image);
        if (paused) {
            window.m_data.slideshowControlsVisible = true;
            QVERIFY(window.handleSlideClick(QPoint(window.bodyRect().width() / 2, window.bodyRect().height() - 10)));
        }
        const auto timer = window.m_slideTimer.id();
        bool observed = false;
        QTimer::singleShot(0, &window, [&]() {
            auto *dialog = qobject_cast<GadgetOptionsDialog *>(QApplication::activeModalWidget());
            if (!dialog) return;
            observed = true;
            if (action == "delay" || action == "cancel") {
                auto *spin = dialog->findChild<QSpinBox *>();
                if (!spin) { observed = false; dialog->reject(); return; }
                spin->setValue(123);
            } else if (action == "shuffle") {
                auto *check = dialog->findChild<QCheckBox *>();
                if (!check) { observed = false; dialog->reject(); return; }
                check->setChecked(true);
            } else if (action.startsWith("transition")) {
                auto *combo = dialog->findChild<QComboBox *>();
                if (!combo) { observed = false; dialog->reject(); return; }
                combo->setCurrentText("none");
            } else if (action == "same-folder") {
                for (auto *line : dialog->findChildren<QLineEdit *>())
                    if (line->text() == pictures.path()) line->setText(pictures.path() + "/.");
            }
            if (action == "cancel") dialog->reject(); else dialog->accept();
        });
        QTimer::singleShot(1000, &window, []() {
            if (auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget())) dialog->reject();
        });
        window.showOptions();
        QVERIFY(observed);
        QCOMPARE(window.m_data.slideImage, image);
        QCOMPARE(window.m_slideIndex, 1);
        QCOMPARE(window.m_slidePaused, paused);
        QCOMPARE(window.m_slideTimer.isActive(), !paused);
        QCOMPARE(window.m_slideTimer.interval(), action == "delay" ? 123000 : 600000);
        if (action != "delay") QCOMPARE(window.m_slideTimer.id(), timer);
        if (action == "cancel") QCOMPARE(window.m_state.settings, state.settings);
        if (action == "shuffle") QVERIFY(window.m_state.settings.value("shuffle").toBool());
        if (action.startsWith("transition")) {
            QCOMPARE(window.m_state.settings.value("transition").toString(), QString("none"));
            QCOMPARE(window.m_slideAnimation.state(), QAbstractAnimation::Stopped);
            QCOMPARE(window.m_data.slideTransition, 1.0);
        }
    }

    void slideshowSkipsUnreadableImages_data()
    {
        QTest::addColumn<QString>("scenario");
        QTest::newRow("corrupt-first") << QStringLiteral("first");
        QTest::newRow("corrupt-next") << QStringLiteral("next");
        QTest::newRow("corrupt-previous") << QStringLiteral("previous");
        QTest::newRow("removed-after-scan") << QStringLiteral("removed");
    }

    void slideshowSkipsUnreadableImages()
    {
        QFETCH(QString, scenario);
        QTemporaryDir pictures;
        QVERIFY(pictures.isValid());
        QImage first(12, 8, QImage::Format_RGB32);
        first.fill(Qt::red);
        QImage last(12, 8, QImage::Format_RGB32);
        last.fill(Qt::blue);
        const auto saveInvalid = [](const QString &path) {
            QFile file(path);
            return file.open(QIODevice::WriteOnly) && file.write("not an image") == 12;
        };
        if (scenario == "first") {
            QVERIFY(saveInvalid(pictures.filePath("a.png")));
            QVERIFY(last.save(pictures.filePath("b.png")));
        } else {
            QVERIFY(first.save(pictures.filePath("a.png")));
            if (scenario == "removed") QVERIFY(first.save(pictures.filePath("b.png")));
            else QVERIFY(saveInvalid(pictures.filePath("b.png")));
            QVERIFY(last.save(pictures.filePath("c.png")));
        }
        const auto *definition = m_manager->definition("org.aero7.gadgets.slideshow");
        QVERIFY(definition);
        GadgetState state;
        state.settings.insert("folder", pictures.path());
        state.settings.insert("transition", "none");
        GadgetWindow window(*definition, state, m_manager.get());
        window.m_slideTimer.stop();
        if (scenario == "first") {
            QCOMPARE(window.m_data.slideImage, last);
            return;
        }
        QCOMPARE(window.m_data.slideImage, first);
        if (scenario == "removed") QVERIFY(QFile::remove(pictures.filePath("b.png")));
        if (scenario == "previous") {
            // Wrap backwards to c, then skip b and return to a.
            window.previousSlide();
            QCOMPARE(window.m_data.slideImage, last);
            window.previousSlide();
            QCOMPARE(window.m_data.slideImage, first);
        } else {
            window.nextSlide();
            QCOMPARE(window.m_data.slideImage, last);
            window.nextSlide();
            QCOMPARE(window.m_data.slideImage, first);
        }
    }

    void slideshowFolderChangeClearsTransition()
    {
        QTemporaryDir pictures;
        QTemporaryDir empty;
        QVERIFY(pictures.isValid() && empty.isValid());
        QImage image(12, 8, QImage::Format_RGB32);
        image.fill(Qt::red);
        QVERIFY(image.save(pictures.filePath("a.png")));
        image.fill(Qt::blue);
        QVERIFY(image.save(pictures.filePath("b.png")));
        const auto *definition = m_manager->definition("org.aero7.gadgets.slideshow");
        QVERIFY(definition);
        GadgetState state;
        state.settings.insert("folder", pictures.path());
        GadgetWindow window(*definition, state, m_manager.get());
        window.nextSlide();
        QVERIFY(!window.m_data.previousSlideImage.isNull());
        QCOMPARE(window.m_slideAnimation.state(), QAbstractAnimation::Running);
        bool observed = false;
        QTimer::singleShot(0, &window, [&]() {
            auto *dialog = qobject_cast<GadgetOptionsDialog *>(QApplication::activeModalWidget());
            if (!dialog) return;
            for (auto *line : dialog->findChildren<QLineEdit *>()) {
                if (line->text() == pictures.path()) { line->setText(empty.path()); observed = true; }
            }
            dialog->accept();
        });
        QTimer::singleShot(1000, &window, []() {
            if (auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget())) dialog->reject();
        });
        window.showOptions();
        QVERIFY(observed);
        QCOMPARE(window.m_state.settings.value("folder").toString(), empty.path());
        QVERIFY(window.m_data.previousSlideImage.isNull());
        QCOMPARE(window.m_slideAnimation.state(), QAbstractAnimation::Stopped);
        QCOMPARE(window.m_data.slideTransition, 1.0);
        QVERIFY(!window.m_slideTimer.isActive());
        QVERIFY(!window.m_data.slideImage.isNull());
    }

    void slideshowAllFilesRemovedRetainsLastImage()
    {
        QTemporaryDir pictures;
        QVERIFY(pictures.isValid());
        QImage image(12, 8, QImage::Format_RGB32);
        image.fill(Qt::red);
        QVERIFY(image.save(pictures.filePath("a.png")));
        const auto *definition = m_manager->definition("org.aero7.gadgets.slideshow");
        QVERIFY(definition);
        GadgetState state;
        state.settings.insert("folder", pictures.path());
        GadgetWindow window(*definition, state, m_manager.get());
        window.m_slideTimer.stop();
        const auto retained = window.m_data.slideImage;
        QVERIFY(QFile::remove(pictures.filePath("a.png")));
        window.nextSlide();
        window.previousSlide();
        QCOMPARE(window.m_data.slideImage, retained);
    }

    void slideshowDelayAndPause_data()
    {
        QTest::addColumn<int>("seconds");
        QTest::addColumn<int>("interval");
        QTest::newRow("minimum") << -5 << 2000;
        QTest::newRow("normal") << 10 << 10000;
        QTest::newRow("overflow") << std::numeric_limits<int>::max() << 3600000;
    }

    void slideshowDelayAndPause()
    {
        QFETCH(int, seconds);
        QFETCH(int, interval);
        QTemporaryDir pictures;
        QVERIFY(pictures.isValid());
        QImage image(12, 8, QImage::Format_RGB32);
        image.fill(Qt::red);
        QVERIFY(image.save(pictures.filePath("a.png")));
        const auto *definition = m_manager->definition("org.aero7.gadgets.slideshow");
        QVERIFY(definition);
        GadgetState state;
        state.settings.insert("folder", pictures.path());
        state.settings.insert("delaySeconds", seconds);
        GadgetWindow window(*definition, state, m_manager.get());
        QCOMPARE(window.m_slideTimer.interval(), interval);
        QVERIFY(window.m_slideTimer.isActive());
        window.m_data.slideshowControlsVisible = true;
        const QPoint pause(window.bodyRect().width() / 2, window.bodyRect().height() - 10);
        QVERIFY(window.handleSlideClick(pause));
        QVERIFY(window.m_slidePaused);
        QVERIFY(!window.m_slideTimer.isActive());
        window.nextSlide();
        QVERIFY(window.m_slidePaused);
        QVERIFY(!window.m_slideTimer.isActive());
        QVERIFY(window.handleSlideClick(pause));
        QVERIFY(!window.m_slidePaused);
        QVERIFY(window.m_slideTimer.isActive());
        QCOMPARE(window.m_slideTimer.interval(), interval);
    }

    void feedClicksOnlyOpenVisibleRows_data()
    {
        QTest::addColumn<QString>("size");
        QTest::newRow("small") << QString("small");
        QTest::newRow("large") << QString("large");
    }

    void feedClicksOnlyOpenVisibleRows()
    {
        QFETCH(QString, size);
        const auto *definition = m_manager->definition("org.aero7.gadgets.feeds");
        QVERIFY(definition);
        FixtureNetwork network;
        RuntimeServices services(nullptr, &network);
        services.m_systemTimer.stop(); services.m_mediaTimer.stop();
        GadgetState state;
        state.size = size;
        GadgetWindow window(*definition, state, m_manager.get(), &services);
        for (int i = 0; i < 30; ++i) {
            window.m_data.feedTitles << QString("Headline %1").arg(i);
            window.m_data.feedLinks << QString("https://qa.invalid/article/%1").arg(i);
        }
        m_openedUrls.clear();
        QDesktopServices::setUrlHandler("https", this, "recordOpenedUrl");
        const int header = size == "large" ? 40 : 33;
        window.openFeedItem(QPoint(10, header - 1));
        window.openFeedItem(QPoint(10, window.bodyRect().bottom() - 5));
        window.openFeedItem(QPoint(window.bodyRect().right() + 1, header + 1));
        const auto ignored = m_openedUrls;
        window.openFeedItem(QPoint(10, header + 1));
        const auto opened = m_openedUrls;
        QDesktopServices::unsetUrlHandler("https");
        QVERIFY(ignored.isEmpty());
        QCOMPARE(opened, QList<QUrl>{QUrl("https://qa.invalid/article/0")});
    }

    void initTestCase()
    {
        QVERIFY(m_profile.isValid());
        qputenv("XDG_CONFIG_HOME", (m_profile.path() + "/config").toUtf8());
        qputenv("XDG_CACHE_HOME", (m_profile.path() + "/cache").toUtf8());
        m_originalPalette = QApplication::palette();
    }

    void init()
    {
        m_caseCache = std::make_unique<QTemporaryDir>();
        QVERIFY(m_caseCache->isValid());
        qputenv("XDG_CACHE_HOME", m_caseCache->path().toUtf8());
        m_manager = std::make_unique<GadgetManager>();
    }

    void cleanup()
    {
        m_manager->ResetLayout();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        m_manager.reset();
        QApplication::setPalette(m_originalPalette);
    }

    void desktopDropKeepsTheReceivingOutput()
    {
        QScreen *screen = QGuiApplication::primaryScreen();
        QVERIFY(screen);
        const QRect geometry = screen->geometry();
        const int x = geometry.width() / 3;
        const int y = geometry.height() / 3;
        QSignalSpy added(m_manager.get(), &GadgetManager::LayoutChanged);
        const QString instance = m_manager->AddGadgetOnScreen(
            QStringLiteral("org.aero7.gadgets.clock"), screen->name(), x, y);
        QVERIFY(!instance.isEmpty());
        QCOMPARE(added.count(), 1);

        GadgetWindow *placed = nullptr;
        for (QWidget *widget : QApplication::topLevelWidgets()) {
            auto *window = qobject_cast<GadgetWindow *>(widget);
            if (window && window->instanceId() == instance) placed = window;
        }
        QVERIFY(placed);
        QCOMPARE(placed->stateForSave().monitor, screen->name());
        QVERIFY(screen->availableGeometry().contains(placed->bodyGeometry()));
        const QPoint actualCenter = placed->bodyGeometry().center();
        const QPoint requestedCenter = geometry.topLeft() + QPoint(x, y);
        QVERIFY((actualCenter - requestedCenter).manhattanLength() <= 2);

        const QString invalid = m_manager->AddGadgetOnScreen(
            QStringLiteral("org.aero7.gadgets.clock"), QStringLiteral("missing-output"), x, y);
        QVERIFY(invalid.isEmpty());
        QCOMPARE(added.count(), 1);
    }

    void weatherRepliesStayWithTheirQuery_data()
    {
        QTest::addColumn<bool>("differentUnits");
        QTest::newRow("different-units") << true;
        QTest::newRow("different-coordinates") << false;
    }

    void weatherRepliesStayWithTheirQuery()
    {
        QFETCH(bool, differentUnits);
        FixtureNetwork network;
        RuntimeServices services(nullptr, &network);
        services.m_systemTimer.stop();
        services.m_mediaTimer.stop();
        const auto *definition = m_manager->definition("org.aero7.gadgets.weather");
        QVERIFY(definition);
        GadgetState state;
        state.settings = {{"location", "Same city name"}, {"latitude", 51.965}, {"longitude", 6.288}, {"unit", "celsius"}};
        GadgetWindow first(*definition, state, m_manager.get(), &services);
        if (differentUnits) state.settings["unit"] = "fahrenheit";
        else state.settings["longitude"] = 7.288;
        GadgetWindow second(*definition, state, m_manager.get(), &services);
        first.updateNetworkData();
        second.updateNetworkData();
        QCOMPARE(network.replies.size(), 2);
        network.replies.last()->finish({{"current", QJsonObject{{"temperature_2m", 68}, {"weather_code", 1}, {"time", "2026-09-10T12:00"}}}});
        QCOMPARE(second.m_data.temperature, 68.0);
        QVERIFY(first.m_data.weatherUpdated.isEmpty());
        network.replies.first()->finish({{"current", QJsonObject{{"temperature_2m", 10}, {"weather_code", 1}, {"time", "2026-09-10T12:00"}}}});
        QCOMPARE(first.m_data.temperature, 10.0);
        QCOMPARE(second.m_data.temperature, 68.0);
    }

    void changedQueryClearsOldReadings_data()
    {
        QTest::addColumn<QString>("kind");
        QTest::newRow("weather") << QString("weather");
        QTest::newRow("currency") << QString("currency");
        QTest::newRow("feed") << QString("feeds");
    }

    void weatherAliasesShareAQuery()
    {
        FixtureNetwork network;
        RuntimeServices services(nullptr, &network);
        services.m_systemTimer.stop(); services.m_mediaTimer.stop();
        const auto *definition = m_manager->definition("org.aero7.gadgets.weather");
        QVERIFY(definition);
        GadgetState state;
        state.settings = {{"location", "My home"}, {"latitude", 51.965}, {"longitude", 6.288}, {"unit", "celsius"}};
        GadgetWindow first(*definition, state, m_manager.get(), &services);
        state.settings["location"] = "Same coordinates, another label";
        GadgetWindow second(*definition, state, m_manager.get(), &services);
        first.updateNetworkData(); second.updateNetworkData();
        network.replies.last()->finish({{"current", QJsonObject{{"temperature_2m", 14}, {"weather_code", 1}, {"time", "2026-09-10T12:00"}}}});
        network.replies.first()->finish({}, QNetworkReply::HostNotFoundError);
        QCOMPARE(first.m_data.temperature, 14.0);
        QCOMPARE(second.m_data.temperature, 14.0);
        QVERIFY(first.m_data.error.isEmpty());
        QVERIFY(second.m_data.error.isEmpty());
    }

    void changedQueryClearsOldReadings()
    {
        QFETCH(QString, kind);
        FixtureNetwork network;
        RuntimeServices services(nullptr, &network);
        services.m_systemTimer.stop();
        services.m_mediaTimer.stop();
        const auto *definition = m_manager->definition("org.aero7.gadgets." + kind);
        QVERIFY(definition);
        GadgetState state;
        state.settings = {{"location", "QA City"}, {"unit", "celsius"}, {"base", "EUR"}, {"target", "USD"},
                          {"feed", "https://qa.invalid/first"}};
        GadgetWindow window(*definition, state, m_manager.get(), &services);
        window.updateNetworkData();
        QCOMPARE(network.replies.size(), 1);
        if (kind == "weather") {
            network.replies.last()->finish({{"current", QJsonObject{{"temperature_2m", 10}, {"weather_code", 1}, {"time", "2026-09-10T12:00"}}}});
            QVERIFY(!window.m_data.weatherUpdated.isEmpty());
            window.m_state.settings["unit"] = "fahrenheit";
            window.updateNetworkData(true);
            QVERIFY(window.m_data.weatherUpdated.isEmpty());
            QCOMPARE(window.m_data.weatherCode, -1);
        } else if (kind == "currency") {
            network.replies.last()->finish({{"date", "2026-09-10"}, {"rates", QJsonObject{{"USD", 1.25}}}});
            QCOMPARE(window.m_data.currencyRate, 1.25);
            window.m_state.settings["target"] = "GBP";
            window.updateNetworkData(true);
            QCOMPARE(window.m_data.currencyRate, 0.0);
            QVERIFY(window.m_data.currencyUpdated.isEmpty());
        } else {
            network.replies.last()->finishBytes("<rss><channel><item><title>Old feed</title><link>https://qa.invalid/article</link></item></channel></rss>");
            QCOMPARE(window.m_data.feedTitles, QStringList{"Old feed"});
            window.m_state.settings["feed"] = "https://qa.invalid/second";
            window.updateNetworkData(true);
            QVERIFY(window.m_data.feedTitles.isEmpty());
            QVERIFY(window.m_data.feedLinks.isEmpty());
        }
        QVERIFY(window.m_data.error.isEmpty());
        QCOMPARE(network.replies.size(), 2);
    }

    void defaultFeedReplyIsAccepted()
    {
        FixtureNetwork network;
        RuntimeServices services(nullptr, &network);
        services.m_systemTimer.stop();
        services.m_mediaTimer.stop();
        const auto *definition = m_manager->definition("org.aero7.gadgets.feeds");
        QVERIFY(definition);
        GadgetWindow window(*definition, {}, m_manager.get(), &services);
        window.updateNetworkData();
        QCOMPARE(network.replies.size(), 1);
        network.replies.last()->finishBytes("<rss><channel><item><title>Default feed</title></item></channel></rss>");
        QCOMPARE(window.m_data.feedTitles, QStringList{"Default feed"});
    }

    void readableGallery_data()
    {
        QTest::addColumn<bool>("dark");
        QTest::newRow("light") << false;
        QTest::newRow("dark") << true;
    }

    void layerDragKeepsPointerAnchor()
    {
        const auto *definition = m_manager->definition(QStringLiteral("org.aero7.gadgets.clock"));
        QVERIFY(definition);
        GadgetState state;
        state.instance = QStringLiteral("drag-regression");
        GadgetWindow window(*definition, state, m_manager.get());
        window.winId();
        window.m_layerShell = LayerShellQt::Window::get(window.windowHandle());
        QVERIFY(window.m_layerShell);
        window.m_desktopScreen = QGuiApplication::primaryScreen();
        window.setDesktopPosition(QPoint(400, 200));
        const QPoint anchor(64, 64);
        QMouseEvent press(QEvent::MouseButtonPress, anchor, anchor,
                          Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(&window, &press);
        QVERIFY(window.m_dragging);
        QVERIFY(window.m_dragPreview);
        // The real input surface stays at its original position; only an
        // input-transparent preview moves. Queued motion events therefore
        // cannot be mistaken for acknowledgement of changed layer margins.
        for (const QPoint expected : {QPoint(380, 200), QPoint(360, 200), QPoint(340, 200)}) {
            const QPoint local = anchor + expected - QPoint(400, 200);
            QMouseEvent move(QEvent::MouseMove, local, local,
                             Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(&window, &move);
            QCOMPARE(window.bodyGeometry().topLeft(), QPoint(400, 200));
            QCOMPARE(window.m_layerShell->margins(), QMargins(400, 200, 0, 0));
            QCOMPARE(window.m_dragTarget, expected);
            QCOMPARE(window.m_dragPreview->pos(), expected);
            QVERIFY(window.m_dragPreview->windowFlags().testFlag(Qt::WindowTransparentForInput));
            QCOMPARE(window.stateForSave().x, 400);
        }
        // Release carries a final movement not seen by mouseMoveEvent.
        const QPoint releasePosition = anchor - QPoint(80, 0);
        QMouseEvent release(QEvent::MouseButtonRelease, releasePosition, releasePosition,
                            Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(&window, &release);
        QVERIFY(!window.m_dragging);
        QVERIFY(!window.m_dragPreview);
        QCOMPARE(window.stateForSave().x, 320);
        QCOMPARE(window.stateForSave().y, 200);
    }

    void previewScalesTheWholeGadget_data()
    {
        QTest::addColumn<QString>("id");
        for (const auto &id : {"calendar", "cpu", "currency", "weather"})
            QTest::newRow(id) << QStringLiteral("org.aero7.gadgets.") + QLatin1String(id);
    }

    void previewScalesTheWholeGadget()
    {
        QFETCH(QString, id);
        const auto *definition = m_manager->definition(id);
        QVERIFY(definition);
        const QPixmap full = GadgetPainter::preview(*definition, definition->smallSize);
        const QPixmap scaled = full.scaled(QSize(64, 64) * full.devicePixelRatio(),
                                          Qt::KeepAspectRatio, Qt::SmoothTransformation);
        QPixmap expected(QSize(64, 64) * full.devicePixelRatio());
        expected.setDevicePixelRatio(full.devicePixelRatio());
        expected.fill(Qt::transparent);
        QPainter painter(&expected);
        const QSize logicalSize = scaled.size() / scaled.devicePixelRatio();
        painter.drawPixmap((64 - logicalSize.width()) / 2, (64 - logicalSize.height()) / 2, scaled);
        painter.end();
        const QPixmap actual = GadgetPainter::preview(*definition, QSize(64, 64));
        QCOMPARE(actual.toImage(), expected.toImage());
        QCOMPARE(actual.devicePixelRatio(), full.devicePixelRatio());
    }

    void unavailableWeatherDoesNotInventAReading_data()
    {
        QTest::addColumn<QString>("error");
        QTest::newRow("waiting") << QString();
        QTest::newRow("offline") << QStringLiteral("Network unavailable");
    }

    void unavailableWeatherDoesNotInventAReading()
    {
        QFETCH(QString, error);
        const auto *definition = m_manager->definition(QStringLiteral("org.aero7.gadgets.weather"));
        QVERIFY(definition);
        GadgetState state;
        state.size = QStringLiteral("small");
        const auto render = [&](const GadgetRenderData &data) {
            QImage image(definition->smallSize, QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);
            QPainter painter(&image);
            GadgetPainter::paint(painter, *definition, state, data, image.rect());
            painter.end();
            return image;
        };
        GadgetRenderData first;
        first.error = error;
        first.temperature = 0;
        first.weatherCode = -1;
        GadgetRenderData second = first;
        second.temperature = 35;
        second.weatherCode = 0;
        QCOMPARE(render(first), render(second));
        // Genuine zero is a valid reading, unlike a failed reply's zero.
        first.error.clear();
        first.weatherCode = 0;
        first.weatherUpdated = QStringLiteral("2026-09-10T12:00");
        second = first;
        second.temperature = 35;
        QVERIFY(render(first) != render(second));
    }

    void cachedReadingsAreLabelled_data()
    {
        QTest::addColumn<QString>("kind");
        QTest::addColumn<QString>("size");
        QTest::newRow("currency-small") << QString("currency") << QString("small");
        QTest::newRow("currency-large") << QString("currency") << QString("large");
        QTest::newRow("weather-small") << QString("weather") << QString("small");
        QTest::newRow("weather-large") << QString("weather") << QString("large");
        QTest::newRow("feeds-small") << QString("feeds") << QString("small");
        QTest::newRow("feeds-large") << QString("feeds") << QString("large");
    }

    void cachedReadingsAreLabelled()
    {
        QFETCH(QString, kind);
        QFETCH(QString, size);
        const auto *definition = m_manager->definition("org.aero7.gadgets." + kind);
        QVERIFY(definition);
        GadgetState state;
        state.size = size;
        GadgetRenderData data;
        data.currencyRate = 1.25;
        data.currencyUpdated = "2026-09-09";
        data.currencyStale = true;
        data.temperature = 0;
        data.weatherCode = 1;
        data.weatherUpdated = "2026-09-09T12:00";
        data.weatherStale = true;
        data.feedTitles = {QStringLiteral("Saved headline")};
        data.feedStale = true;
        data.feedLoaded = true;
        const QSize dimensions = size == "large" ? definition->largeSize : definition->smallSize;
        const auto render = [&]() {
            QByteArray output;
            QBuffer buffer(&output);
            buffer.open(QIODevice::WriteOnly);
            QSvgGenerator svg;
            svg.setOutputDevice(&buffer);
            svg.setSize(dimensions);
            svg.setViewBox(QRect(QPoint(), dimensions));
            QPainter painter(&svg);
            GadgetPainter::paint(painter, *definition, state, data, QRect(QPoint(), dimensions));
            painter.end();
            return output;
        };
        QVERIFY2(render().contains("Cached"), "Cached readings must not look like current provider results");
        data.currencyStale = false;
        data.weatherStale = false;
        data.feedStale = false;
        QVERIFY(!render().contains("Cached"));
        if (kind == "feeds") {
            data.feedTitles.clear();
            QVERIFY(render().contains("No headlines"));
            data.error = "Network unavailable";
            QVERIFY(render().contains("Feed unavailable"));
            QVERIFY(!render().contains("Cached"));
        }
    }

    void calendarWeekStartSurvivesOptions_data()
    {
        QTest::addColumn<int>("day");
        QTest::newRow("monday") << 1;
        QTest::newRow("sunday") << 7;
    }

    void calendarWeekStartSurvivesOptions()
    {
        QFETCH(int, day);
        const auto *definition = m_manager->definition(QStringLiteral("org.aero7.gadgets.calendar"));
        QVERIFY(definition);
        const QJsonObject settings{{QStringLiteral("firstDay"), day},
                                   {QStringLiteral("highlightToday"), true},
                                   {QStringLiteral("weekNumbers"), false}};
        GadgetOptionsDialog dialog(*definition, settings);
        QCOMPARE(dialog.settings(), settings);
    }

    void puzzleDifficultySurvivesOptions_data()
    {
        QTest::addColumn<int>("difficulty");
        QTest::newRow("three") << 3;
        QTest::newRow("four") << 4;
        QTest::newRow("five") << 5;
    }

    void puzzleDifficultySurvivesOptions()
    {
        QFETCH(int, difficulty);
        const auto *definition = m_manager->definition(QStringLiteral("org.aero7.gadgets.picturepuzzle"));
        QVERIFY(definition);
        const QJsonObject settings{{QStringLiteral("image"), QStringLiteral("aero7-flower")},
                                   {QStringLiteral("difficulty"), difficulty},
                                   {QStringLiteral("customImage"), QString()}};
        GadgetOptionsDialog dialog(*definition, settings);
        QCOMPARE(dialog.settings(), settings);
    }

    void readableGallery()
    {
        QFETCH(bool, dark);
        QPalette palette = m_originalPalette;
        palette.setColor(QPalette::Window, dark ? QColor("#31363b") : QColor("#f0f0f0"));
        palette.setColor(QPalette::Base, dark ? QColor("#232629") : QColor("#ffffff"));
        for (const auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText}) {
            palette.setColor(role, dark ? QColor("#eff0f1") : QColor("#111111"));
        }
        palette.setColor(QPalette::Link, dark ? QColor("#8ab4f8") : QColor("#0655bd"));
        QApplication::setPalette(palette);
        GadgetGallery gallery(m_manager.get());
        gallery.show();
        QTest::qWait(25);
        auto *list = gallery.findChild<QListWidget *>();
        QVERIFY(list);
        QCOMPARE(list->count(), 8);
        QVERIFY2(contrast(list->palette().color(QPalette::Text), list->palette().color(QPalette::Base)) >= 4.5,
                 "Unselected gadget names must remain readable on the gallery background");
        QToolButton *details = nullptr;
        for (auto *button : gallery.findChildren<QToolButton *>()) {
            if (button->isCheckable()) details = button;
        }
        QVERIFY(details);
        details->click();
        QTest::qWait(10);
        QLabel *description = nullptr;
        for (auto *label : gallery.findChildren<QLabel *>()) {
            if (label->text().contains("Double-click or drag")) description = label;
        }
        QVERIFY(description);
        QVERIFY(description->isVisible());
        QVERIFY2(contrast(description->palette().color(QPalette::WindowText), gallery.palette().color(QPalette::Window)) >= 4.5,
                 "Expanded gadget details must be readable in both palettes");
    }

    void darkContextMenuReadable()
    {
        QPalette palette = m_originalPalette;
        palette.setColor(QPalette::Window, QColor("#31363b"));
        palette.setColor(QPalette::WindowText, QColor("#eff0f1"));
        palette.setColor(QPalette::Text, QColor("#eff0f1"));
        palette.setColor(QPalette::ButtonText, QColor("#eff0f1"));
        QApplication::setPalette(palette);
        GadgetGallery gallery(m_manager.get());
        gallery.show();
        auto *list = gallery.findChild<QListWidget *>();
        QVERIFY(list);
        QTest::qWait(25);
        double ratio = 0;
        bool observed = false;
        QTimer::singleShot(25, &gallery, [&] {
            auto *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
            if (menu) {
                observed = true;
                ratio = contrast(menu->palette().color(QPalette::Text), menu->palette().color(QPalette::Window));
                menu->close();
            }
        });
        // Bound a failed popup observation so the nested menu loop cannot hang QA.
        QTimer::singleShot(1000, &gallery, [] {
            if (auto *popup = QApplication::activePopupWidget()) popup->close();
        });
        QVERIFY(QMetaObject::invokeMethod(&gallery, "showItemMenu", Qt::DirectConnection,
                                         Q_ARG(QPoint, list->visualItemRect(list->item(0)).center())));
        QVERIFY(observed);
        QVERIFY2(ratio >= 4.5, "The Add context menu must not inherit light text on its fixed light background");
    }

    void enterAddsOneGadgetWithoutOpeningWebsite_data()
    {
        QTest::addColumn<int>("key");
        QTest::newRow("return") << int(Qt::Key_Return);
        QTest::newRow("keypad-enter") << int(Qt::Key_Enter);
    }

    void enterAddsOneGadgetWithoutOpeningWebsite()
    {
        QFETCH(int, key);
        GadgetGallery gallery(m_manager.get());
        auto *list = gallery.findChild<QListWidget *>();
        auto *online = gallery.findChild<QPushButton *>();
        QVERIFY(list);
        QVERIFY(online);
        // Never open an external URL even against the known-broken baseline.
        QObject::disconnect(online, SIGNAL(clicked(bool)), nullptr, nullptr);
        QSignalSpy website(online, &QPushButton::clicked);
        QSignalSpy added(m_manager.get(), &GadgetManager::LayoutChanged);
        gallery.show();
        list->setCurrentRow(1); // Clock is local and does not request network data.
        list->setFocus();
        QTest::qWait(25);
        QTest::keyClick(list, Qt::Key(key));
        QCOMPARE(added.count(), 1);
        QCOMPARE(website.count(), 0);
        QVERIFY(gallery.isVisible());
    }

private:
    QList<QUrl> m_openedUrls;
    QTemporaryDir m_profile;
    std::unique_ptr<QTemporaryDir> m_caseCache;
    QPalette m_originalPalette;
    std::unique_ptr<GadgetManager> m_manager;
};

QTEST_MAIN(GalleryTest)
#include "GalleryTest.moc"
