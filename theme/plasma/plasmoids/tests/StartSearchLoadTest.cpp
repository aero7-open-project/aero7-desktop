/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <QQmlComponent>
#include <QQmlEngine>
#include <QtTest>

class StartSearchLoadTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void shippedSearchCompiles()
    {
        // Compile the entire shipped component, not an extracted copy of its
        // functions. This catches duplicate ids that qmllint can miss.
        // Applet context, search results and launching are exercised in the VM.
        QQmlEngine engine;
        QQmlComponent component(&engine, QUrl::fromLocalFile(QStringLiteral(SEARCH_QML_PATH)),
                                QQmlComponent::PreferSynchronous);
        QTRY_VERIFY_WITH_TIMEOUT(component.status() != QQmlComponent::Loading, 10000);
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    }
};
QTEST_MAIN(StartSearchLoadTest)
#include "StartSearchLoadTest.moc"
