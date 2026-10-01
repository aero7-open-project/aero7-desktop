/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <QFile>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QtTest>
#include <memory>

class PanelShadowMarginsTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void geometry_data()
    {
        QTest::addColumn<QSizeF>("panelSize");
        QTest::addColumn<QRectF>("backgroundRect");
        QTest::addColumn<QMarginsF>("expected");
        QTest::newRow("docked") << QSizeF(1920, 40) << QRectF(0, 0, 1920, 40) << QMarginsF(0, 0, 0, 0);
        QTest::newRow("floating-horizontal") << QSizeF(1920, 60) << QRectF(8, 10, 1904, 40) << QMarginsF(-8, -10, -8, -10);
        QTest::newRow("floating-vertical") << QSizeF(60, 1080) << QRectF(10, 10, 40, 1060) << QMarginsF(-10, -10, -10, -10);
        QTest::newRow("fractional-animation") << QSizeF(100.5, 100.5) << QRectF(1.25, 2.5, 90.25, 85.5) << QMarginsF(-1.25, -2.5, -9, -12.5);
        QTest::newRow("background-outside-panel") << QSizeF(100, 100) << QRectF(-2, -3, 110, 120) << QMarginsF(2, 3, 8, 17);
    }

    void geometry()
    {
        QFETCH(QSizeF, panelSize);
        QFETCH(QRectF, backgroundRect);
        QFETCH(QMarginsF, expected);
        QFile source(QStringLiteral(PANEL_QML_PATH));
        QVERIFY(source.open(QIODevice::ReadOnly));
        const QString panel = QString::fromUtf8(source.readAll());
        // Execute the bindings from the actual shipped view. Isolate only its
        // geometry inputs; loading the complete view is covered by the VM replay.
        QRegularExpression property(QStringLiteral(
            "^\\s*(?:readonly\\s+)?property\\s+real\\s+(?:left|top|right|bottom)ShadowMargin\\s*:[^\\n]+"),
            QRegularExpression::MultilineOption);
        auto matches = property.globalMatch(panel);
        QString declarations;
        int count = 0;
        while (matches.hasNext()) {
            declarations += matches.next().captured() + QLatin1Char('\n');
            ++count;
        }
        QCOMPARE(count, 4);
        QQmlEngine engine;
        QQmlComponent component(&engine);
        component.setData((QStringLiteral("import QtQuick\nItem {\n") + declarations
            + QStringLiteral("Item { id: floatingTranslucentItem; objectName: \"background\" }\n}")).toUtf8(), QUrl());
        std::unique_ptr<QObject> object(component.create());
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *root = qobject_cast<QQuickItem *>(object.get());
        auto *background = object->findChild<QQuickItem *>(QStringLiteral("background"));
        QVERIFY(root);
        QVERIFY(background);
        root->setSize(panelSize);
        background->setPosition(backgroundRect.topLeft());
        background->setSize(backgroundRect.size());
        const auto margin = [&](const char *name) { return root->property(name).toReal(); };
        QCOMPARE(margin("leftShadowMargin"), expected.left());
        QCOMPARE(margin("topShadowMargin"), expected.top());
        QCOMPARE(margin("rightShadowMargin"), expected.right());
        QCOMPARE(margin("bottomShadowMargin"), expected.bottom());

        // PanelView uses these exact notify signals for border/shadow updates.
        QSignalSpy left(root, SIGNAL(leftShadowMarginChanged()));
        QSignalSpy top(root, SIGNAL(topShadowMarginChanged()));
        QSignalSpy right(root, SIGNAL(rightShadowMarginChanged()));
        QSignalSpy bottom(root, SIGNAL(bottomShadowMarginChanged()));
        QVERIFY(left.isValid() && top.isValid() && right.isValid() && bottom.isValid());
        background->setX(background->x() + 3);
        QCOMPARE(margin("leftShadowMargin"), expected.left() - 3);
        QCOMPARE(margin("rightShadowMargin"), expected.right() + 3);
        QCOMPARE(left.count(), 1);
        QCOMPARE(right.count(), 1);
        QCOMPARE(top.count(), 0);
        QCOMPARE(bottom.count(), 0);
        background->setY(background->y() + 2);
        background->setWidth(background->width() + 7);
        background->setHeight(background->height() + 5);
        root->setWidth(root->width() + 10);
        root->setHeight(root->height() + 9);
        QCOMPARE(margin("leftShadowMargin"), expected.left() - 3);
        QCOMPARE(margin("topShadowMargin"), expected.top() - 2);
        // Right: +3 x offset +7 background width -10 panel width.
        // Bottom: +2 y offset +5 background height -9 panel height.
        QCOMPARE(margin("rightShadowMargin"), expected.right());
        QCOMPARE(margin("bottomShadowMargin"), expected.bottom() - 2);
        QCOMPARE(left.count(), 1);
        QCOMPARE(top.count(), 1);
        QCOMPARE(right.count(), 3);
        QCOMPARE(bottom.count(), 3);
    }
};
QTEST_MAIN(PanelShadowMarginsTest)
#include "PanelShadowMarginsTest.moc"
