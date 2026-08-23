// SPDX-License-Identifier: MIT
#include "PinsStore.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

class PinsStoreTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void defaultsAreLoadedAndNormalized()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString defaults = directory.filePath(QStringLiteral("defaults.json"));
        QFile file(defaults);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(R"({"schema":1,"pins":["applications:a.desktop","applications:a.desktop","  applications:b.desktop  ",""]})");
        file.close();

        PinsStore store(directory.filePath(QStringLiteral("config/taskbar.json")), defaults);
        QCOMPARE(store.load(), QStringList({QStringLiteral("applications:a.desktop"), QStringLiteral("applications:b.desktop")}));
    }

    void savedPinsOverrideDefaultsAtomically()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        PinsStore store(directory.filePath(QStringLiteral("config/taskbar.json")),
                        directory.filePath(QStringLiteral("missing-defaults.json")));
        QVERIFY(store.save({QStringLiteral("applications:qterminal.desktop"), QStringLiteral("applications:qterminal.desktop")}));
        QCOMPARE(store.load(), QStringList({QStringLiteral("applications:qterminal.desktop")}));
        QFile config(directory.filePath(QStringLiteral("config/taskbar.json")));
        QVERIFY(config.open(QIODevice::ReadOnly));
        QVERIFY(config.readAll().contains("\"schema\": 1"));
    }

    void corruptUserConfigurationFallsBackToDefaults()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString defaults = directory.filePath(QStringLiteral("defaults.json"));
        QFile defaultsFile(defaults);
        QVERIFY(defaultsFile.open(QIODevice::WriteOnly));
        defaultsFile.write(R"({"schema":1,"pins":["applications:org.aero7.controlpanel.desktop"]})");
        defaultsFile.close();

        const QString config = directory.filePath(QStringLiteral("taskbar.json"));
        QFile configFile(config);
        QVERIFY(configFile.open(QIODevice::WriteOnly));
        configFile.write("{ this is not valid JSON");
        configFile.close();

        PinsStore store(config, defaults);
        QCOMPARE(store.load(), QStringList({QStringLiteral("applications:org.aero7.controlpanel.desktop")}));
    }

    void intentionallyEmptyUserConfigurationStaysEmpty()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString defaults = directory.filePath(QStringLiteral("defaults.json"));
        QFile defaultsFile(defaults);
        QVERIFY(defaultsFile.open(QIODevice::WriteOnly));
        defaultsFile.write(R"({"schema":1,"pins":["applications:qterminal.desktop"]})");
        defaultsFile.close();

        PinsStore store(directory.filePath(QStringLiteral("config/taskbar.json")), defaults);
        QVERIFY(store.save({}));
        QCOMPARE(store.load(), QStringList{});
    }
};

QTEST_MAIN(PinsStoreTest)
#include "test_pins_store.moc"
