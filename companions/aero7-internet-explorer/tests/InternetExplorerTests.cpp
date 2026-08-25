#include <aero7compat/BrowserConfig.h>
#include <aero7compat/BrowserRegistry.h>
#include <aero7compat/DefaultApplications.h>
#include <aero7compat/InternetExplorer.h>

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

using namespace Aero7::Compat;

class InternetExplorerTests : public QObject {
    Q_OBJECT
private:
    static QString writeDesktop(const QString &root, const QString &id,
                                const QString &name,
                                const QString &mime = QStringLiteral(
                                    "text/html;x-scheme-handler/http;x-scheme-handler/https;"),
                                const QString &categories = QStringLiteral(
                                    "Network;WebBrowser;"),
                                bool actions = true)
    {
        QDir().mkpath(root);
        const QString path = QDir(root).filePath(id);
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return {};
        QByteArray contents = QByteArrayLiteral(
            "[Desktop Entry]\nType=Application\nExec=/usr/bin/true %U\nIcon=internet-web-browser\n");
        contents += "Name=" + name.toUtf8() + '\n';
        contents += "MimeType=" + mime.toUtf8() + '\n';
        contents += "Categories=" + categories.toUtf8() + '\n';
        if (actions) {
            contents += QByteArrayLiteral(
                "Actions=new-window;private-window;\n"
                "[Desktop Action new-window]\nName=New Window\nExec=/usr/bin/true\n"
                "[Desktop Action private-window]\nName=New Private Window\nExec=/usr/bin/true\n");
        }
        file.write(contents);
        file.close();
        return path;
    }

    static QString writeXdgMimeStub(const QString &root)
    {
        const QString path = QDir(root).filePath(QStringLiteral("xdg-mime-stub"));
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return {};
        file.write(R"SH(#!/bin/bash
set -eu
if [[ "$1" == query ]]; then
    case "$3" in
        x-scheme-handler/http) printf '%s\n' "${AERO7_TEST_HTTP_DEFAULT:-}" ;;
        x-scheme-handler/https) printf '%s\n' "${AERO7_TEST_HTTPS_DEFAULT:-}" ;;
        text/html) printf '%s\n' "${AERO7_TEST_HTML_DEFAULT:-}" ;;
    esac
    exit 0
fi
printf '%s\t%s\n' "$2" "$3" >> "$AERO7_TEST_XDG_LOG"
)SH");
        file.close();
        file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner
                            | QFileDevice::ExeOwner);
        return path;
    }

private slots:
    void detectsKnownUnknownAndFlatpakBrowsers()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        const QString native = QDir(temp.path()).filePath(QStringLiteral("native"));
        const QString flatpak = QDir(temp.path()).filePath(QStringLiteral("flatpak"));
        QVERIFY(!writeDesktop(native, QStringLiteral("librewolf.desktop"),
                              QStringLiteral("LibreWolf")).isEmpty());
        QVERIFY(!writeDesktop(native, QStringLiteral("firefox.desktop"),
                              QStringLiteral("Firefox")).isEmpty());
        QVERIFY(!writeDesktop(native, QStringLiteral("chromium.desktop"),
                              QStringLiteral("Chromium")).isEmpty());
        QVERIFY(!writeDesktop(flatpak, QStringLiteral("org.example.Web.desktop"),
                              QStringLiteral("Example Web")).isEmpty());
        QVERIFY(!writeDesktop(native, QStringLiteral("avahi.desktop"),
                              QStringLiteral("Server Browser"), {},
                              QStringLiteral("Network;"), false).isEmpty());
        QVERIFY(!writeDesktop(native,
                              QString::fromLatin1(DefaultApplications::WrapperDesktopId),
                              QStringLiteral("Internet Explorer")).isEmpty());

        BrowserRegistry registry({native, flatpak});
        const auto browsers = registry.installedBrowsers();
        QCOMPARE(browsers.size(), 4);
        QVERIFY(registry.find(QStringLiteral("librewolf.desktop")).supportsPrivateMode());
        QVERIFY(registry.find(QStringLiteral("firefox.desktop")).supportsNewWindow());
        QVERIFY(registry.find(QStringLiteral("org.example.Web.desktop")).isValid());
        QVERIFY(!registry.find(QStringLiteral("avahi.desktop")).isValid());
        QVERIFY(!registry.find(QString::fromLatin1(
            DefaultApplications::WrapperDesktopId)).isValid());
    }

    void handlesNoAndMultipleBrowsers()
    {
        QTemporaryDir temp;
        BrowserRegistry empty({QDir(temp.path()).filePath(QStringLiteral("none"))});
        QCOMPARE(empty.installedBrowsers().size(), 0);

        const QString apps = QDir(temp.path()).filePath(QStringLiteral("apps"));
        writeDesktop(apps, QStringLiteral("firefox.desktop"), QStringLiteral("Firefox"));
        writeDesktop(apps, QStringLiteral("chromium.desktop"), QStringLiteral("Chromium"));
        BrowserRegistry multiple({apps});
        QCOMPARE(multiple.installedBrowsers().size(), 2);
    }

    void validatesConfigurationAndRejectsRecursion()
    {
        QTemporaryDir temp;
        BrowserConfig config(QDir(temp.path()).filePath(QStringLiteral("user.conf")),
                             QDir(temp.path()).filePath(QStringLiteral("policy.conf")));
        QVERIFY(config.setBackendId(QStringLiteral("firefox.desktop")));
        QCOMPARE(config.backendId(), QStringLiteral("firefox.desktop"));
        QVERIFY(!config.setBackendId(QString::fromLatin1(
            DefaultApplications::WrapperDesktopId)));
        QVERIFY(!config.setBackendId(QStringLiteral("../../malicious.desktop")));
        QCOMPARE(config.backendId(), QStringLiteral("firefox.desktop"));
    }

    void honorsLockedPolicy()
    {
        QTemporaryDir temp;
        const QString policyPath = QDir(temp.path()).filePath(QStringLiteral("policy.conf"));
        QFile policy(policyPath);
        QVERIFY(policy.open(QIODevice::WriteOnly));
        policy.write("[Policy]\nBackend=chromium.desktop\nAllowUserChange=false\n");
        policy.close();
        BrowserConfig config(QDir(temp.path()).filePath(QStringLiteral("user.conf")),
                             policyPath);
        QVERIFY(config.policyLocksBackend());
        QCOMPARE(config.backendId(), QStringLiteral("chromium.desktop"));
        QVERIFY(!config.setBackendId(QStringLiteral("firefox.desktop")));
    }

    void resolvesConfiguredMissingDefaultAndRecursionCases()
    {
        QTemporaryDir temp;
        const QString apps = QDir(temp.path()).filePath(QStringLiteral("apps"));
        writeDesktop(apps, QStringLiteral("firefox.desktop"), QStringLiteral("Firefox"));
        writeDesktop(apps, QStringLiteral("chromium.desktop"), QStringLiteral("Chromium"));
        const QString xdg = writeXdgMimeStub(temp.path());
        const QString log = QDir(temp.path()).filePath(QStringLiteral("xdg.log"));
        qputenv("AERO7_TEST_XDG_LOG", log.toUtf8());

        BrowserConfig missingConfig(
            QDir(temp.path()).filePath(QStringLiteral("missing.conf")),
            QDir(temp.path()).filePath(QStringLiteral("policy.conf")));
        QVERIFY(missingConfig.setBackendId(QStringLiteral("librewolf.desktop")));
        InternetExplorer missing(BrowserRegistry({apps}), missingConfig, xdg);
        QCOMPARE(missing.resolveBackend(false).status,
                 ResolutionStatus::ConfiguredBackendMissing);

        qputenv("AERO7_TEST_HTTP_DEFAULT", QByteArrayLiteral("firefox.desktop"));
        BrowserConfig defaultConfig(
            QDir(temp.path()).filePath(QStringLiteral("default.conf")),
            QDir(temp.path()).filePath(QStringLiteral("policy.conf")));
        InternetExplorer fromDefault(BrowserRegistry({apps}), defaultConfig, xdg);
        const auto adopted = fromDefault.resolveBackend(false);
        QCOMPARE(adopted.status, ResolutionStatus::Ready);
        QCOMPARE(adopted.backend.desktopId, QStringLiteral("firefox.desktop"));
        QVERIFY(adopted.adoptedSystemDefault);

        qputenv("AERO7_TEST_HTTP_DEFAULT",
                QByteArray(DefaultApplications::WrapperDesktopId));
        BrowserConfig recursionConfig(
            QDir(temp.path()).filePath(QStringLiteral("recursion.conf")),
            QDir(temp.path()).filePath(QStringLiteral("policy.conf")));
        InternetExplorer recursion(BrowserRegistry({apps}), recursionConfig, xdg);
        QCOMPARE(recursion.resolveBackend(false).status,
                 ResolutionStatus::SelectionRequired);
    }

    void setsAndRestoresWebDefaultsWithoutShellEvaluation()
    {
        QTemporaryDir temp;
        const QString log = QDir(temp.path()).filePath(QStringLiteral("xdg.log"));
        qputenv("AERO7_TEST_XDG_LOG", log.toUtf8());
        qputenv("AERO7_TEST_HTTP_DEFAULT", QByteArrayLiteral("firefox.desktop"));
        qputenv("AERO7_TEST_HTTPS_DEFAULT", QByteArrayLiteral("firefox.desktop"));
        qputenv("AERO7_TEST_HTML_DEFAULT", QByteArrayLiteral("firefox.desktop"));
        BrowserConfig config(QDir(temp.path()).filePath(QStringLiteral("user.conf")),
                             QDir(temp.path()).filePath(QStringLiteral("policy.conf")));
        DefaultApplications defaults(config, writeXdgMimeStub(temp.path()));
        QString error;
        QVERIFY2(defaults.setInternetExplorerDefault(&error), qPrintable(error));
        QFile calls(log);
        QVERIFY(calls.open(QIODevice::ReadOnly));
        const QByteArray setCalls = calls.readAll();
        calls.close();
        QCOMPARE(setCalls.count(DefaultApplications::WrapperDesktopId), 3);
        QCOMPARE(config.previousDefault(QStringLiteral("text/html")),
                 QStringLiteral("firefox.desktop"));

        QFile::remove(log);
        qputenv("AERO7_TEST_HTTP_DEFAULT",
                QByteArray(DefaultApplications::WrapperDesktopId));
        qputenv("AERO7_TEST_HTTPS_DEFAULT",
                QByteArray(DefaultApplications::WrapperDesktopId));
        qputenv("AERO7_TEST_HTML_DEFAULT",
                QByteArray(DefaultApplications::WrapperDesktopId));
        QVERIFY2(defaults.restorePreviousDefaults(&error), qPrintable(error));
        QVERIFY(calls.open(QIODevice::ReadOnly));
        QCOMPARE(calls.readAll().count("firefox.desktop"), 3);
    }

    void normalizesUrlsAndLocalHtmlSafely()
    {
        QTemporaryDir temp;
        const QString html = QDir(temp.path()).filePath(
            QStringLiteral("page with spaces.html"));
        QFile file(html);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("<html></html>");
        file.close();

        const auto urls = InternetExplorer::normalizeUrls({
            QStringLiteral("https://example.com/a%20b?q=one%20two"),
            QStringLiteral("http://example.org"), html,
            QStringLiteral("https://example.net/$(touch%20never)")});
        QCOMPARE(urls.size(), 4);
        QCOMPARE(urls.at(0).toString(QUrl::FullyEncoded),
                 QStringLiteral("https://example.com/a%20b?q=one%20two"));
        QCOMPARE(urls.at(1).scheme(), QStringLiteral("http"));
        QVERIFY(urls.at(2).isLocalFile());
        QCOMPARE(urls.at(2).toLocalFile(), html);
        QCOMPARE(urls.at(3).scheme(), QStringLiteral("https"));
    }

    void capturesNormalLaunchInputs_data()
    {
        QTest::addColumn<QStringList>("arguments");
        QTest::addColumn<int>("expectedCount");
        QTest::addColumn<QString>("expectedFirst");

        QTest::newRow("without-url") << QStringList{} << 0 << QString{};
        QTest::newRow("http-url")
            << QStringList{QStringLiteral("http://example.org")} << 1
            << QStringLiteral("http://example.org");
        QTest::newRow("https-url")
            << QStringList{QStringLiteral("https://example.org/secure")} << 1
            << QStringLiteral("https://example.org/secure");
        QTest::newRow("multiple-urls")
            << QStringList{QStringLiteral("https://example.org/one"),
                           QStringLiteral("https://example.net/two")} << 2
            << QStringLiteral("https://example.org/one");
        QTest::newRow("encoded-url")
            << QStringList{QStringLiteral("https://example.org/a%20b?q=one%20two")} << 1
            << QStringLiteral("https://example.org/a%20b?q=one%20two");
        QTest::newRow("local-html")
            << QStringList{QStringLiteral("__LOCAL_HTML__")} << 1
            << QStringLiteral("file");
    }

    void capturesNormalLaunchInputs()
    {
        QFETCH(QStringList, arguments);
        QFETCH(int, expectedCount);
        QFETCH(QString, expectedFirst);
        QTemporaryDir temp;
        if (arguments == QStringList{QStringLiteral("__LOCAL_HTML__")}) {
            const QString html = QDir(temp.path()).filePath(
                QStringLiteral("local page.html"));
            QFile local(html);
            QVERIFY(local.open(QIODevice::WriteOnly));
            local.write("<html></html>");
            local.close();
            arguments = {html};
        }
        const QString apps = QDir(temp.path()).filePath(QStringLiteral("apps"));
        writeDesktop(apps, QStringLiteral("firefox.desktop"),
                     QStringLiteral("Firefox"));
        const QString capture = QDir(temp.path()).filePath(QStringLiteral("capture.json"));
        qputenv("AERO7_IE_CAPTURE_LAUNCH", capture.toUtf8());
        BrowserConfig config(QDir(temp.path()).filePath(QStringLiteral("user.conf")),
                             QDir(temp.path()).filePath(QStringLiteral("policy.conf")));
        InternetExplorer ie(BrowserRegistry({apps}), config,
                            writeXdgMimeStub(temp.path()));
        const BrowserBackend backend = ie.installedBrowsers().first();
        const LaunchResult result = ie.launch(backend, arguments, LaunchMode::Normal);
        QVERIFY2(result.started, qPrintable(result.error));
        QFile file(capture);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QJsonObject object = QJsonDocument::fromJson(file.readAll()).object();
        const QJsonArray urls = object.value(QStringLiteral("urls")).toArray();
        QCOMPARE(urls.size(), expectedCount);
        if (expectedFirst == QStringLiteral("file")) {
            QVERIFY(urls.first().toString().startsWith(QStringLiteral("file:")));
        } else if (expectedCount > 0) {
            QCOMPARE(urls.first().toString(), expectedFirst);
        }
        qunsetenv("AERO7_IE_CAPTURE_LAUNCH");
    }

    void capturesLaunchArgumentsAndModes()
    {
        QTemporaryDir temp;
        const QString apps = QDir(temp.path()).filePath(QStringLiteral("apps"));
        const QString desktopPath = writeDesktop(
            apps, QStringLiteral("firefox.desktop"), QStringLiteral("Firefox"));
        BrowserBackend backend;
        backend.displayName = QStringLiteral("Firefox");
        backend.desktopId = QStringLiteral("firefox.desktop");
        backend.desktopFilePath = desktopPath;
        backend.iconName = QStringLiteral("firefox");
        backend.privateAction = QStringLiteral("private-window");
        const QString capture = QDir(temp.path()).filePath(QStringLiteral("capture.json"));
        qputenv("AERO7_IE_CAPTURE_LAUNCH", capture.toUtf8());

        BrowserConfig config(QDir(temp.path()).filePath(QStringLiteral("user.conf")),
                             QDir(temp.path()).filePath(QStringLiteral("policy.conf")));
        InternetExplorer ie(BrowserRegistry({apps}), config,
                            writeXdgMimeStub(temp.path()));
        const LaunchResult launched = ie.launch(backend,
            {QStringLiteral("https://example.com/one%20two"),
             QStringLiteral("https://example.org")},
            LaunchMode::PrivateWindow);
        QVERIFY2(launched.started, qPrintable(launched.error));
        QFile captureFile(capture);
        QVERIFY(captureFile.open(QIODevice::ReadOnly));
        const QJsonObject object = QJsonDocument::fromJson(captureFile.readAll()).object();
        QCOMPARE(object.value(QStringLiteral("desktopId")).toString(),
                 QStringLiteral("firefox.desktop"));
        QCOMPARE(object.value(QStringLiteral("mode")).toString(),
                 QStringLiteral("private-window"));
        QCOMPARE(object.value(QStringLiteral("urls")).toArray().size(), 2);
        qunsetenv("AERO7_IE_CAPTURE_LAUNCH");
    }

    void appendsUrlsToDesktopActionsWithoutFieldCodes()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        const QString capture = QDir(temp.path()).filePath(
            QStringLiteral("action-arguments.txt"));
        const QString executable = QDir(temp.path()).filePath(
            QStringLiteral("capture-action"));
        QFile script(executable);
        QVERIFY(script.open(QIODevice::WriteOnly | QIODevice::Truncate));
        script.write("#!/bin/bash\nprintf '%s\\n' \"$@\" > \"$AERO7_ACTION_CAPTURE\"\n");
        script.close();
        QVERIFY(script.setPermissions(QFileDevice::ReadOwner
                                      | QFileDevice::WriteOwner
                                      | QFileDevice::ExeOwner));

        const QString apps = QDir(temp.path()).filePath(QStringLiteral("apps"));
        QDir().mkpath(apps);
        const QString desktopPath = QDir(apps).filePath(
            QStringLiteral("chromium.desktop"));
        QFile desktop(desktopPath);
        QVERIFY(desktop.open(QIODevice::WriteOnly | QIODevice::Truncate));
        desktop.write(QStringLiteral(
            "[Desktop Entry]\nType=Application\nName=Chromium\n"
            "Exec=%1 %U\nIcon=chromium\nCategories=Network;WebBrowser;\n"
            "MimeType=text/html;x-scheme-handler/http;x-scheme-handler/https;\n"
            "Actions=new-private-window;\n"
            "[Desktop Action new-private-window]\nName=New Private Window\n"
            "Exec=%1 --incognito\n")
            .arg(executable).toUtf8());
        desktop.close();

        qputenv("AERO7_ACTION_CAPTURE", capture.toUtf8());
        BrowserConfig config(QDir(temp.path()).filePath(QStringLiteral("user.conf")),
                             QDir(temp.path()).filePath(QStringLiteral("policy.conf")));
        InternetExplorer ie(BrowserRegistry({apps}), config,
                            writeXdgMimeStub(temp.path()));
        const BrowserBackend backend = ie.installedBrowsers().first();
        const LaunchResult launched = ie.launch(
            backend, {QStringLiteral("https://example.com/private-test")},
            LaunchMode::PrivateWindow);
        QVERIFY2(launched.started, qPrintable(launched.error));
        QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(capture), 2000);
        QFile captured(capture);
        QVERIFY(captured.open(QIODevice::ReadOnly));
        QCOMPARE(QString::fromUtf8(captured.readAll()).split(QLatin1Char('\n'),
                                                          Qt::SkipEmptyParts),
                 QStringList({QStringLiteral("--incognito"),
                              QStringLiteral("https://example.com/private-test")}));
        qunsetenv("AERO7_ACTION_CAPTURE");
    }
};

QTEST_GUILESS_MAIN(InternetExplorerTests)
#include "InternetExplorerTests.moc"
