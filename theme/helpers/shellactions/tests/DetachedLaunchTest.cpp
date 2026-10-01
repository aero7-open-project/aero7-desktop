#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTemporaryDir>
#include <QTest>
using namespace Qt::StringLiterals;

class DetachedLaunchTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void survivesClosedCallerPipes_data()
    {
        QTest::addColumn<QString>("action");
        QTest::addColumn<QString>("program");
        QTest::addColumn<QStringList>("arguments");
        QTest::newRow("taskbar") << u"taskbar-properties"_s << u"control"_s
            << QStringList({u"--page"_s, u"taskbar-start-menu"_s});
        QTest::newRow("start") << u"start-properties"_s << u"control"_s
            << QStringList({u"--page"_s, u"taskbar-start-menu"_s, u"--tab"_s, u"start-menu"_s});
        QTest::newRow("explorer") << u"explorer"_s << u"aero7-dolphin"_s << QStringList();
        QTest::newRow("task-manager") << u"task-manager"_s << u"tux-manager"_s << QStringList();
    }
    void survivesClosedCallerPipes()
    {
        QFETCH(QString, action);
        QFETCH(QString, program);
        QFETCH(QStringList, arguments);
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(QFile::link(QStringLiteral(FIXTURE_PATH), dir.filePath(program)));
        const QString marker = dir.filePath(u"completed.json"_s);
        {
            QProcess launcher;
            auto env = QProcessEnvironment::systemEnvironment();
            env.insert(u"PATH"_s, dir.path() + u":"_s + env.value(u"PATH"_s));
            env.insert(u"AERO7_QA_LAUNCH_MARKER"_s, marker);
            launcher.setProcessEnvironment(env);
            launcher.start(QStringLiteral(LAUNCHER_PATH), {action});
            QVERIFY(launcher.waitForFinished(5000));
            QCOMPARE(launcher.exitStatus(), QProcess::NormalExit);
            QCOMPARE(launcher.exitCode(), 0);
#ifdef EXPECT_JOURNAL_UNAVAILABLE
            QVERIFY(launcher.readAllStandardError().contains("journal unavailable"));
#endif
            // Destroy QProcess now, before the fixture writes to stdout/stderr.
        }
        QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(marker), 2000);
        QFile result(marker);
        QVERIFY(result.open(QIODevice::ReadOnly));
        QCOMPARE(QJsonDocument::fromJson(result.readAll()).array(),
                 QJsonArray::fromStringList(arguments));
    }
    void missingExecutableFails()
    {
        QProcess launcher;
        auto env = QProcessEnvironment::systemEnvironment();
        env.insert(u"PATH"_s, u"/aero7-qa-nonexistent-directory"_s);
        launcher.setProcessEnvironment(env);
        launcher.start(QStringLiteral(LAUNCHER_PATH), {u"taskbar-properties"_s});
        QVERIFY(launcher.waitForFinished(5000));
        QCOMPARE(launcher.exitCode(), 1);
    }
};
QTEST_GUILESS_MAIN(DetachedLaunchTest)
#include "DetachedLaunchTest.moc"
