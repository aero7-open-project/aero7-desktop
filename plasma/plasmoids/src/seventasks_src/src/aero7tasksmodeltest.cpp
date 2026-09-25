/*
    SPDX-FileCopyrightText: 2026 Aero7 Open Project
    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#include "aero7tasksmodel.h"

#include <QFile>
#include <QGuiApplication>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

class Aero7TasksModelTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void draggedPinsAreSavedBeforeShellRestart()
    {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const QString statePath = temporary.filePath(QStringLiteral("taskbar-state.ini"));
        qputenv("AERO7_TASKBAR_STATE_CONFIG", statePath.toUtf8());
        qputenv("AERO7_IE_TASKBAR_CONFIG", temporary.filePath(QStringLiteral("browser.ini")).toUtf8());
        qputenv("AERO7_IE_TASKBAR_POLICY", temporary.filePath(QStringLiteral("policy.ini")).toUtf8());
        const QStringList original{QStringLiteral("applications:qterminal.desktop"),
                                   QStringLiteral("applications:steam.desktop"),
                                   QStringLiteral("applications:discord.desktop")};
        const QStringList reordered{original.at(2), original.at(0), original.at(1)};
        {
            Aero7TasksModel model;
            model.setSortMode(TaskManager::TasksModel::SortManual);
            model.setSeparateLaunchers(true);
            model.setLaunchInPlace(true);
            model.setShellLauncherList(original);
            QCoreApplication::processEvents();
            if (QGuiApplication::platformName() == QLatin1String("offscreen") && model.rowCount() == 0) {
                QSKIP("The offscreen platform does not expose taskbar rows; run this case in a graphical session");
            }
            QTRY_COMPARE(model.rowCount(), 3);
            QVERIFY(model.move(2, 0));
            QCOMPARE(model.shellLauncherList(), reordered);
            QSettings saved(statePath, QSettings::IniFormat);
            QCOMPARE(saved.value(QStringLiteral("Pinned/Launchers")).toStringList(), reordered);
            QVERIFY(!model.move(0, 0));
        }
        Aero7TasksModel afterShellRestart;
        QCOMPARE(afterShellRestart.shellLauncherList(), reordered);
    }

    void pinnedLayoutSurvivesPanelAndShellRestart()
    {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const QString statePath = temporary.filePath(QStringLiteral("taskbar-state.ini"));
        qputenv("AERO7_TASKBAR_STATE_CONFIG", statePath.toUtf8());
        qputenv("AERO7_IE_TASKBAR_CONFIG", temporary.filePath(QStringLiteral("browser.ini")).toUtf8());
        qputenv("AERO7_IE_TASKBAR_POLICY", temporary.filePath(QStringLiteral("policy.ini")).toUtf8());
        const QStringList pinned{QStringLiteral("applications:qterminal.desktop"),
                                 QStringLiteral("applications:org.aero7.FileExplorer.desktop")};
        {
            Aero7TasksModel firstPanel;
            firstPanel.restoreOrAdoptLaunchers(pinned);
            QCOMPARE(firstPanel.shellLauncherList(), pinned);
            Aero7TasksModel staleSecondPanel;
            staleSecondPanel.restoreOrAdoptLaunchers({QStringLiteral("applications:old.desktop")});
            QCOMPARE(staleSecondPanel.shellLauncherList(), pinned);
            firstPanel.setShellLauncherList({pinned.at(1), pinned.at(0)});
        }
        Aero7TasksModel afterShellRestart;
        afterShellRestart.restoreOrAdoptLaunchers({QStringLiteral("applications:old.desktop")});
        QCOMPARE(afterShellRestart.shellLauncherList(), (QStringList{pinned.at(1), pinned.at(0)}));
        afterShellRestart.setShellLauncherList({});
        QSettings saved(statePath, QSettings::IniFormat);
        QVERIFY(saved.contains(QStringLiteral("Pinned/Launchers")));
        Aero7TasksModel emptyAfterRestart;
        emptyAfterRestart.restoreOrAdoptLaunchers(pinned);
        QVERIFY(emptyAfterRestart.shellLauncherList().isEmpty());
    }

    void canonicalLauncherIsStableAcrossBackends()
    {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const QString config = temporary.filePath(QStringLiteral("internet-explorer.conf"));
        const QString policy = temporary.filePath(QStringLiteral("policy.conf"));
        qputenv("AERO7_IE_TASKBAR_CONFIG", config.toUtf8());
        qputenv("AERO7_IE_TASKBAR_POLICY", policy.toUtf8());
        qputenv("AERO7_TASKBAR_STATE_CONFIG", temporary.filePath(QStringLiteral("taskbar-state.ini")).toUtf8());

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
