/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <QFile>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QSignalSpy>
#include <QtTest>
#include <memory>

class TaskTooltipTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void roleLifetime_data()
    {
        QTest::addColumn<QString>("replacement");
        QTest::newRow("roles-removed") << QStringLiteral("({})");
        QTest::newRow("model-null") << QStringLiteral("null");
        QTest::newRow("model-undefined") << QStringLiteral("undefined");
        QTest::newRow("partial-launcher") << QStringLiteral("({ index: 0, IsLauncher: true })");
    }

    void roleLifetime()
    {
        QFETCH(QString, replacement);
        QFile source(QStringLiteral(TASK_QML_PATH));
        QVERIFY(source.open(QIODevice::ReadOnly));
        const QString text = QString::fromUtf8(source.readAll());
        const auto start = text.indexOf(QStringLiteral("    function updateToolTipBindings() {"));
        QVERIFY(start >= 0);
        const auto end = text.indexOf(QStringLiteral("    // END TOOLTIP CODE"), start);
        QVERIFY(end > start);
        // Execute the shipped function with its shared tooltip and typed roles.
        // The full Plasma delegate and real task transitions remain VM tests.
        const QString fixture = QStringLiteral(R"(
import QtQml
QtObject {
    id: task
    property var model: ({})
    function modelIndex() { return model?.index ?? -1; }
    property QtObject dragArea: QtObject { property bool containsMouse: false }
    property QtObject taskThumbnail: QtObject {
        property QtObject parentTask
        property bool demandsAttention: false
        property bool minimized: false
        property string display: ""
        property var icon
        property bool active: false
        property bool startup: false
        property var windows
        property var modelIndex
        property var taskIndex
        property int pidParent: 0
        property url launcherUrl: ""
        property bool isGroupParent: false
        property bool taskHovered: false
    }
    function populate() {
        model = { IsDemandingAttention: true, IsMinimized: true,
            display: "Explorer", decoration: "folder", IsActive: true,
            IsStartup: true, WinIdList: [42], index: 3, AppPid: 123,
            LauncherUrlWithoutIcon: "applications:org.aero7.FileExplorer.desktop",
            IsGroupParent: true };
    }
    function replaceModel() { model = )") + replacement + QStringLiteral("; }\n")
            + text.mid(start, end - start) + QStringLiteral("}\n");
        QQmlEngine engine;
        QSignalSpy warnings(&engine, &QQmlEngine::warnings);
        QQmlComponent component(&engine);
        component.setData(fixture.toUtf8(), QUrl(QStringLiteral("file:///TaskTooltipFixture.qml")));
        std::unique_ptr<QObject> task(component.create());
        QVERIFY2(task, qPrintable(component.errorString()));
        QVERIFY(QMetaObject::invokeMethod(task.get(), "populate"));
        QVERIFY(QMetaObject::invokeMethod(task.get(), "updateToolTipBindings"));
        auto *tooltip = task->property("taskThumbnail").value<QObject *>();
        QVERIFY(tooltip);
        QCOMPARE(tooltip->property("display").toString(), QStringLiteral("Explorer"));
        QCOMPARE(tooltip->property("pidParent").toInt(), 123);
        QVERIFY(tooltip->property("active").toBool());
        QCOMPARE(tooltip->property("windows").value<QJSValue>().toVariant().toList().size(), 1);
        QCOMPARE(warnings.count(), 0);

        QVERIFY(QMetaObject::invokeMethod(task.get(), "replaceModel"));
        QCOMPARE(warnings.count(), 0);
        for (const char *role : {"demandsAttention", "minimized", "active", "startup", "isGroupParent"}) {
            QVERIFY2(!tooltip->property(role).toBool(), role);
        }
        QCOMPARE(tooltip->property("display").toString(), QString());
        QCOMPARE(tooltip->property("pidParent").toInt(), 0);
        QCOMPARE(tooltip->property("launcherUrl").toUrl(), QUrl());
        QCOMPARE(tooltip->property("icon").toString(), QString());
        QCOMPARE(tooltip->property("windows").value<QJSValue>().toVariant().toList().size(), 0);
        QCOMPARE(tooltip->property("taskIndex").toInt(), replacement.contains(QStringLiteral("index: 0")) ? 0 : -1);

        // Existing bindings must recover on the next model without rebinding.
        QVERIFY(QMetaObject::invokeMethod(task.get(), "populate"));
        QCOMPARE(tooltip->property("display").toString(), QStringLiteral("Explorer"));
        QCOMPARE(tooltip->property("pidParent").toInt(), 123);
        QVERIFY(tooltip->property("active").toBool());
        QCOMPARE(warnings.count(), 0);
    }
};
QTEST_MAIN(TaskTooltipTest)
#include "TaskTooltipTest.moc"
