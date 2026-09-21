/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <QFile>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QtTest>
#include <memory>

class GroupPreviewSizeTest : public QObject
{
    Q_OBJECT

    static QString read(const char *name)
    {
        QFile file(qEnvironmentVariable("AERO7_TEST_PREVIEW_QML_DIR", QStringLiteral(PREVIEW_QML_DIR))
            + QLatin1Char('/') + QString::fromLatin1(name));
        if (!file.open(QIODevice::ReadOnly)) {
            return {};
        }
        return QString::fromUtf8(file.readAll());
    }

private Q_SLOTS:
    void overflowClose()
    {
        const QString source = read("WindowListDelegate.qml");
        const auto start = source.indexOf(QStringLiteral("    function closeTask()"));
        const auto end = source.indexOf(QStringLiteral("    hoverEnabled:"), start);
        QVERIFY(start > 0 && end > start);
        const QString fixture = QStringLiteral(R"(
import QtQuick
Item {
    property int modelIndex: 7
    property QtObject tasksModel: QtObject {
        property int closedIndex: -1
        function requestClose(index) { closedIndex = index; }
    }
    property QtObject root: QtObject {
        property QtObject parentTask: QtObject {
            property bool hidden: false
            function hideImmediately() { hidden = true; }
        }
    }
    readonly property int closedIndex: tasksModel.closedIndex
    readonly property bool hidden: root.parentTask.hidden
)") + source.mid(start, end - start) + QStringLiteral("}\n");
        QQmlEngine engine;
        QSignalSpy warnings(&engine, &QQmlEngine::warnings);
        QQmlComponent component(&engine);
        component.setData(fixture.toUtf8(), QUrl(QStringLiteral("file:///OverflowCloseFixture.qml")));
        std::unique_ptr<QObject> object(component.create());
        QVERIFY2(object, qPrintable(component.errorString()));
        QVERIFY(QMetaObject::invokeMethod(object.get(), "closeTask"));
        QCOMPARE(object->property("closedIndex").toInt(), 7);
        // A group row closes only its window; the surviving rows stay open.
        QCOMPARE(object->property("hidden").toBool(), false);
        QCOMPARE(warnings.count(), 0);
    }

    void overflowHover()
    {
        const QString source = read("WindowListDelegate.qml");
        const auto binding = QRegularExpression(QStringLiteral("opacity: contentMa.containsMouse[^\\n]*")).match(source);
        QVERIFY(binding.hasMatch());
        const QString fixture = QStringLiteral(R"(
import QtQuick
Item {
    property QtObject tasks: QtObject { property bool iconsOnly: false }
    property QtObject root: QtObject { property bool taskHovered: true }
    Item { id: contentMa; property bool containsMouse: false }
    Item { id: closeMa; property bool containsMouse: false }
    function hoverRow() { contentMa.containsMouse = true; }
    function hoverClose() { contentMa.containsMouse = false; closeMa.containsMouse = true; }
    function leaveRow() { contentMa.containsMouse = false; closeMa.containsMouse = false; }
)") + binding.captured() + QStringLiteral("\n}\n");
        QQmlEngine engine;
        QSignalSpy warnings(&engine, &QQmlEngine::warnings);
        QQmlComponent component(&engine);
        component.setData(fixture.toUtf8(), QUrl(QStringLiteral("file:///OverflowHoverFixture.qml")));
        std::unique_ptr<QObject> object(component.create());
        QVERIFY2(object, qPrintable(component.errorString()));
        QCOMPARE(object->property("opacity").toReal(), 0.0);
        QVERIFY(QMetaObject::invokeMethod(object.get(), "hoverRow"));
        QCOMPARE(object->property("opacity").toReal(), 1.0);
        QVERIFY(QMetaObject::invokeMethod(object.get(), "hoverClose"));
        QCOMPARE(object->property("opacity").toReal(), 1.0);
        QVERIFY(QMetaObject::invokeMethod(object.get(), "leaveRow"));
        QCOMPARE(object->property("opacity").toReal(), 0.0);
        QCOMPARE(warnings.count(), 0);
    }

    void naturalSize_data()
    {
        QTest::addColumn<bool>("caption");
        QTest::addColumn<bool>("media");
        QTest::newRow("caption") << true << false;
        QTest::newRow("media") << false << true;
        QTest::newRow("caption-and-media") << true << true;
    }

    void naturalSize()
    {
        QFETCH(bool, caption);
        QFETCH(bool, media);
        const QString source = read("WindowThumbnail.qml");
        const auto start = source.indexOf(QStringLiteral("    implicitHeight:"));
        const auto end = source.indexOf(QStringLiteral("    onImplicitHeightChanged:"), start);
        QVERIFY(start > 0 && end > start);
        const QString fixture = QStringLiteral(R"(
import QtQuick
Item {
    property real thumbnailHeight: 94
    property real margins: 32
    property QtObject tasks: QtObject { property bool iconsOnly: )")
            + (caption ? QStringLiteral("true") : QStringLiteral("false")) + QStringLiteral(R"( }
    Item { id: header; height: 99; implicitHeight: 16 }
    Item { id: mprisControls; height: 77; implicitHeight: 28; property bool active: )")
            + (media ? QStringLiteral("true") : QStringLiteral("false")) + QStringLiteral(R"( }
    function resizeAssignedChildren() { header.height = 200; mprisControls.height = 300; }
    function resizeNaturalChildren() { header.implicitHeight = 32; mprisControls.implicitHeight = 40; }
)") + source.mid(start, end - start) + QStringLiteral("}\n");
        QQmlEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("Kirigami"), QVariantMap{
            {QStringLiteral("Units"), QVariantMap{{QStringLiteral("smallSpacing"), 4}}}});
        QQmlComponent component(&engine);
        component.setData(fixture.toUtf8(), QUrl(QStringLiteral("file:///NaturalPreviewSizeFixture.qml")));
        std::unique_ptr<QObject> object(component.create());
        QVERIFY2(object, qPrintable(component.errorString()));
        const qreal expected = 94 + 32 + (caption ? 16 : 0) + (media ? 28 - 8 : 0);
        QCOMPARE(object->property("implicitHeight").toReal(), expected);
        QVERIFY(QMetaObject::invokeMethod(object.get(), "resizeAssignedChildren"));
        QCOMPARE(object->property("implicitHeight").toReal(), expected);
        QVERIFY(QMetaObject::invokeMethod(object.get(), "resizeNaturalChildren"));
        QCOMPARE(object->property("implicitHeight").toReal(), expected + (caption ? 16 : 0) + (media ? 12 : 0));
    }

    void geometry_data()
    {
        QTest::addColumn<bool>("wayland");
        QTest::addColumn<bool>("listMode");
        QTest::newRow("wayland-thumbnails") << true << false;
        QTest::newRow("x11-thumbnails") << false << false;
        QTest::newRow("wayland-list") << true << true;
        QTest::newRow("x11-list") << false << true;
    }

    void geometry()
    {
        QFETCH(bool, wayland);
        QFETCH(bool, listMode);
        const QString group = read("GroupThumbnails.qml");
        const auto sizes = QRegularExpression(QStringLiteral("property (?:int|real) maxThumbnailWidth:")).match(group);
        QVERIFY(sizes.hasMatch());
        const auto end = group.indexOf(QStringLiteral("            interactive:"), sizes.capturedStart());
        QVERIFY(end > sizes.capturedStart());
        const QString sizeLogic = group.mid(sizes.capturedStart(), end - sizes.capturedStart());

        const QString thumbnail = read("WindowThumbnail.qml");
        const auto start = thumbnail.indexOf(QStringLiteral("    onImplicitHeightChanged:"));
        const auto stop = thumbnail.indexOf(QStringLiteral("    hoverEnabled:"), start);
        QVERIFY(start > 0 && stop > start);
        const QString thumbnailLogic = thumbnail.mid(start, stop - start);

        const QString row = read("WindowListDelegate.qml");
        const auto rowStart = row.indexOf(QStringLiteral("    onImplicitWidthChanged:"));
        const auto rowStop = row.indexOf(QStringLiteral("    function closeTask()"), rowStart);
        QVERIFY(rowStart > 0 && rowStop > rowStart);
        QString rowLogic = row.mid(rowStart, rowStop - rowStart);
        // Natural row height is provided by the fixture, not its visual theme.
        rowLogic.remove(QRegularExpression(QStringLiteral("    implicitHeight:[^\\n]*\\n")));
        const auto destruction = QRegularExpression(QStringLiteral("    Component.onDestruction:[^\\n]+"))
            .match(listMode ? row : thumbnail);
        QVERIFY(destruction.hasMatch());

        // Run the production group aggregation and delegate equalization code.
        // Substitute only model data and visual dependencies; the full visual
        // components are exercised separately by the installed VM replay.
        const QString fixture = QStringLiteral(R"(
import QtQuick
ListView {
    id: thumbnailList
    width: 1024; height: 768
    property bool isList: )") + (listMode ? QStringLiteral("true") : QStringLiteral("false"))
            + QStringLiteral(R"(
    model: rows
    orientation: isList ? ListView.Vertical : ListView.Horizontal
    ListModel { id: rows }
    function seed() {
        rows.append({ naturalWidth: 196, naturalHeight: 142 });
        rows.append({ naturalWidth: 196, naturalHeight: 180 });
        forceLayout(); updateMaxSize();
    }
    function grow() {
        rows.setProperty(0, "naturalWidth", 230);
        rows.setProperty(0, "naturalHeight", 210);
        forceLayout();
    }
    function shrink() {
        rows.setProperty(0, "naturalWidth", 170);
        rows.setProperty(0, "naturalHeight", 120);
        forceLayout();
    }
    function removeMaximum() { rows.remove(1); forceLayout(); updateMaxSize(); }
    function empty() { rows.clear(); forceLayout(); updateMaxSize(); }
    delegate: Item {
        id: thumbnailRoot
        property bool isGroupDelegate: true
        implicitWidth: model.naturalWidth
        implicitHeight: model.naturalHeight
)") + (listMode ? rowLogic : thumbnailLogic) + destruction.captured() + QStringLiteral("\n    }\n")
            + sizeLogic + QStringLiteral("}\n");
        QQmlEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("KWindowSystem"), QVariantMap{
            {QStringLiteral("isPlatformWayland"), wayland}});
        QSignalSpy warnings(&engine, &QQmlEngine::warnings);
        QQmlComponent component(&engine);
        component.setData(fixture.toUtf8(), QUrl(QStringLiteral("file:///GroupPreviewSizeFixture.qml")));
        std::unique_ptr<QObject> object(component.create());
        QVERIFY2(object, qPrintable(component.errorString()));
        QVERIFY(QMetaObject::invokeMethod(object.get(), "seed"));
        QTRY_COMPARE_WITH_TIMEOUT(object->property("maxThumbnailHeight").toReal(), 180.0, 500);
        QTRY_COMPARE_WITH_TIMEOUT(object->property("maxThumbnailWidth").toReal(), 196.0, 500);
        QVERIFY(QMetaObject::invokeMethod(object.get(), "grow"));
        QTRY_COMPARE_WITH_TIMEOUT(object->property("maxThumbnailHeight").toReal(), 210.0, 500);
        QTRY_COMPARE_WITH_TIMEOUT(object->property("maxThumbnailWidth").toReal(), 230.0, 500);
        QVERIFY(QMetaObject::invokeMethod(object.get(), "shrink"));
        QTRY_COMPARE_WITH_TIMEOUT(object->property("maxThumbnailHeight").toReal(), 180.0, 500);
        QTRY_COMPARE_WITH_TIMEOUT(object->property("maxThumbnailWidth").toReal(), 196.0, 500);
        QVERIFY(QMetaObject::invokeMethod(object.get(), "removeMaximum"));
        QTRY_COMPARE_WITH_TIMEOUT(object->property("maxThumbnailHeight").toReal(), 120.0, 500);
        QTRY_COMPARE_WITH_TIMEOUT(object->property("maxThumbnailWidth").toReal(), 170.0, 500);
        QVERIFY(QMetaObject::invokeMethod(object.get(), "empty"));
        QTRY_COMPARE_WITH_TIMEOUT(object->property("maxThumbnailHeight").toReal(), 0.0, 500);
        QTRY_COMPARE_WITH_TIMEOUT(object->property("maxThumbnailWidth").toReal(), 0.0, 500);
        QCOMPARE(warnings.count(), 0);
        // Deferred measurements must not execute against a destroyed view.
        QVERIFY(QMetaObject::invokeMethod(object.get(), "seed"));
        object.reset();
        QCoreApplication::processEvents();
        QCOMPARE(warnings.count(), 0);
    }
};
QTEST_MAIN(GroupPreviewSizeTest)
#include "GroupPreviewSizeTest.moc"
