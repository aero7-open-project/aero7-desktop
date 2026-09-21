/*
    SPDX-FileCopyrightText: 2026 Aero7 Open Project
    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#include "aero7tasksmodel.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

class Aero7TasksModelTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void canonicalLauncherIsStableAcrossBackends()
    {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const QString config = temporary.filePath(QStringLiteral("internet-explorer.conf"));
        const QString policy = temporary.filePath(QStringLiteral("policy.conf"));
        qputenv("AERO7_IE_TASKBAR_CONFIG", config.toUtf8());
        qputenv("AERO7_IE_TASKBAR_POLICY", policy.toUtf8());

        QFile file(config);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("[Browser]\nBackend=firefox.desktop\n");
        file.close();

        Aero7TasksModel model;
        const QString shellLauncher = QStringLiteral(
            "applications:aero7-internet-explorer.desktop");
        model.setShellLauncherList({shellLauncher});
        QCOMPARE(model.internetExplorerBackend(), QStringLiteral("firefox.desktop"));
        QCOMPARE(model.shellLauncherList(), QStringList{shellLauncher});
        QCOMPARE(model.TaskManager::TasksModel::launcherList(),
                 QStringList{QStringLiteral("applications:firefox.desktop")});
        QCOMPARE(model.launcherPosition(QUrl(shellLauncher)), 0);

        QSignalSpy backendChanged(&model,
                                  &Aero7TasksModel::internetExplorerBackendChanged);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write("[Browser]\nBackend=chromium.desktop\n");
        file.close();
        QTRY_COMPARE(model.internetExplorerBackend(), QStringLiteral("chromium.desktop"));
        QVERIFY(!backendChanged.isEmpty());
        QCOMPARE(model.shellLauncherList(), QStringList{shellLauncher});
        QCOMPARE(model.TaskManager::TasksModel::launcherList(),
                 QStringList{QStringLiteral("applications:chromium.desktop")});
        QCOMPARE(model.launcherPosition(QUrl(shellLauncher)), 0);
    }
};

QTEST_MAIN(Aero7TasksModelTest)
#include "aero7tasksmodeltest.moc"
