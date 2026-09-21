// SPDX-License-Identifier: GPL-3.0-or-later
#include "KWalletBackend.h"
#include "VaultWindow.h"
#include <QApplication>
#include <QDBusConnection>
#include <QDBusContext>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDBusReply>
#include <QFile>
#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>
#include <memory>

namespace {
const QString servicePath = "/org/freedesktop/secrets";
const QString serviceName = "org.kde.secretservicecompat";
const QString collectionPath = servicePath + "/collection/Aero7_20Credentials";
const QString otherPath = servicePath + "/collection/Other";
}

class MockCollection : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.Secret.Collection")
    Q_PROPERTY(QString Label READ label)
    Q_PROPERTY(bool Locked READ locked)
public:
    bool isLocked = true;
    QString label() const { return "Aero7 Credentials"; }
    bool locked() const { return isLocked; }
};

class MalformedCollection : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.Secret.Collection")
    Q_PROPERTY(QString Label READ label)
    Q_PROPERTY(QString Locked READ locked)
public:
    QString label() const { return "Aero7 Credentials"; }
    QString locked() const { return "false"; }
};

class MockSecrets : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.Secret.Service")
    Q_PROPERTY(QList<QDBusObjectPath> Collections READ collections)
public:
    MockCollection collection;
    MalformedCollection malformedCollection;
    bool loseObjectOnLock = false;
    bool refuseLock = false;
    bool deferUnlock = false;
    int unlockCalls = 0;
    QList<QDBusMessage> pendingUnlocks;
    QList<QDBusObjectPath> collections() const { return {QDBusObjectPath(collectionPath)}; }
public slots:
    QList<QDBusObjectPath> Unlock(const QList<QDBusObjectPath> &paths, QDBusObjectPath &prompt) {
        ++unlockCalls;
        if (deferUnlock) {
            setDelayedReply(true);
            pendingUnlocks.append(message());
            return {};
        }
        collection.isLocked = false;
        prompt = QDBusObjectPath("/");
        return paths;
    }
    QList<QDBusObjectPath> Lock(const QList<QDBusObjectPath> &paths, QDBusObjectPath &prompt) {
        prompt = QDBusObjectPath("/");
        if (refuseLock) return {};
        collection.isLocked = true;
        if (loseObjectOnLock) QDBusConnection::sessionBus().unregisterObject(collectionPath);
        emit CollectionChanged(QDBusObjectPath(collectionPath));
        return paths;
    }
    void LoseObjectOnNextLock() { loseObjectOnLock = true; }
    void RefuseNextLock() { refuseLock = true; }
    void ExternalLock() {
        // KWallet 6.29 emits the service signal, not PropertiesChanged(Locked).
        collection.isLocked = true;
        emit CollectionChanged(QDBusObjectPath(collectionPath));
    }
    void OtherCollectionChanged() { emit CollectionChanged(QDBusObjectPath(otherPath)); }
    void UnlockedCollectionChanged() { emit CollectionChanged(QDBusObjectPath(collectionPath)); }
    void DeleteCollection() { emit CollectionDeleted(QDBusObjectPath(collectionPath)); }
    void LoseCollectionObject() {
        QDBusConnection::sessionBus().unregisterObject(collectionPath);
        emit CollectionChanged(QDBusObjectPath(collectionPath));
    }
    void LoseService() { QDBusConnection::sessionBus().unregisterService(serviceName); }
    bool MalformLockProperty() {
        auto bus = QDBusConnection::sessionBus();
        bus.unregisterObject(collectionPath);
        return bus.registerObject(collectionPath, &malformedCollection,
                                  QDBusConnection::ExportAllProperties);
    }
    void MalformedLockSignal() { sendLockSignal(QString("false")); }
    void ValidUnlockedSignal() { sendLockSignal(false); }
    void DeferUnlocks() { deferUnlock = true; }
    int UnlockCalls() const { return unlockCalls; }
    void FinishUnlocksInReverseOrder() {
        collection.isLocked = false;
        while (!pendingUnlocks.isEmpty()) {
            const auto request = pendingUnlocks.takeLast();
            QDBusConnection::sessionBus().send(request.createReply({
                QVariant::fromValue(QList<QDBusObjectPath>{QDBusObjectPath(collectionPath)}),
                QVariant::fromValue(QDBusObjectPath("/"))}));
        }
    }
private:
    void sendLockSignal(const QVariant &value) {
        auto message = QDBusMessage::createSignal(collectionPath,
            "org.freedesktop.DBus.Properties", "PropertiesChanged");
        message.setArguments({QString("org.freedesktop.Secret.Collection"),
            QVariantMap{{"Locked", value}}, QStringList{}});
        QDBusConnection::sessionBus().send(message);
    }
signals:
    void CollectionChanged(const QDBusObjectPath &path);
    void CollectionDeleted(const QDBusObjectPath &path);
};

class MockWallet : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.KWallet")
public slots:
    int open(const QString &, qlonglong, const QString &) { return 42; }
    bool hasFolder(int, const QString &, const QString &) { return true; }
    QStringList entryList(int, const QString &, const QString &) { return {}; }
    int close(int, bool, const QString &) { return 0; }
};

class KWalletBackendTest : public QObject {
    Q_OBJECT
    QProcess daemon;
    QTemporaryDir config;
    void command(const QString &method) {
        QDBusInterface service(serviceName, servicePath,
                               "org.freedesktop.Secret.Service");
        const auto reply = service.call(method);
        QVERIFY2(reply.type() != QDBusMessage::ErrorMessage, qPrintable(reply.errorMessage()));
    }
private slots:
    void initTestCase() {
        QVERIFY(config.isValid());
        // This test runs on an isolated bus; never touch host account policy.
        qputenv("XDG_CONFIG_HOME", config.path().toUtf8());
        qputenv("XDG_CONFIG_DIRS", config.path().toUtf8());
        QFile policy(config.filePath("kwalletrc"));
        QVERIFY(policy.open(QIODevice::WriteOnly));
        policy.write("[Wallet]\nEnabled=true\n");
    }
    void init() {
        daemon.start(QCoreApplication::applicationFilePath(), {"--mock-service"});
        QVERIFY(daemon.waitForStarted());
        QVERIFY(daemon.waitForReadyRead());
        QCOMPARE(daemon.readLine().trimmed(), QByteArray("READY"));
    }
    void cleanup() {
        daemon.terminate();
        QVERIFY(daemon.waitForFinished());
    }
    void serviceCollectionChangeLocksBackend() {
        KWalletBackend backend;
        backend.unlock(0);
        QTRY_COMPARE(backend.state(), VaultBackend::Unlocked);
        command("ExternalLock");
        QTRY_COMPARE_WITH_TIMEOUT(backend.state(), VaultBackend::Locked, 2000);
    }
    void usesKWalletServiceWithoutGenericAlias() {
        KWalletBackend backend;
        backend.unlock(0);
        QTRY_COMPARE_WITH_TIMEOUT(backend.state(), VaultBackend::Unlocked, 2000);
    }
    void malformedLockStateFailsClosed_data() {
        QTest::addColumn<QString>("route");
        QTest::newRow("before-unlock") << QString("unlock");
        QTest::newRow("before-entry-read") << QString("read");
        QTest::newRow("service-change") << QString("service");
        QTest::newRow("properties-change") << QString("signal");
    }
    void malformedLockStateFailsClosed() {
        QFETCH(QString, route);
        KWalletBackend backend;
        VaultWindow window(&backend);
        if (route != "unlock") {
            backend.unlock(0);
            QTRY_COMPARE(backend.state(), VaultBackend::Unlocked);
        }
        if (route == "signal") {
            command("MalformedLockSignal");
        } else {
            QDBusInterface service(serviceName, servicePath,
                                   "org.freedesktop.Secret.Service");
            QDBusReply<bool> replaced = service.call("MalformLockProperty");
            QVERIFY(replaced.isValid() && replaced.value());
            if (route == "unlock") backend.unlock(0);
            else if (route == "read") QVERIFY(backend.entries().isEmpty());
            else command("UnlockedCollectionChanged");
        }
        QTRY_COMPARE_WITH_TIMEOUT(backend.state(), VaultBackend::Locked, 2000);
        QVERIFY(!window.findChild<QPushButton *>("addCredential")->isEnabled());
    }
    void validFalseLockSignalKeepsUnlocked() {
        KWalletBackend backend;
        backend.unlock(0);
        QTRY_COMPARE(backend.state(), VaultBackend::Unlocked);
        command("ValidUnlockedSignal");
        QTest::qWait(100);
        QCOMPARE(backend.state(), VaultBackend::Unlocked);
    }
    void overlappingUnlockReplies_data() {
        QTest::addColumn<bool>("destroyFirst");
        QTest::newRow("both-survive-reversed-replies") << false;
        QTest::newRow("first-client-destroyed") << true;
    }
    void overlappingUnlockReplies() {
        QFETCH(bool, destroyFirst);
        command("DeferUnlocks");
        auto first = std::make_unique<KWalletBackend>();
        KWalletBackend second;
        first->unlock(0);
        second.unlock(0);
        QCOMPARE(first->state(), VaultBackend::Opening);
        QCOMPARE(second.state(), VaultBackend::Opening);
        QDBusInterface service(serviceName, servicePath,
                               "org.freedesktop.Secret.Service");
        QTRY_COMPARE(QDBusReply<int>(service.call("UnlockCalls")).value(), 2);
        if (destroyFirst) first.reset();
        command("FinishUnlocksInReverseOrder");
        QTRY_COMPARE(second.state(), VaultBackend::Unlocked);
        if (first) QTRY_COMPARE(first->state(), VaultBackend::Unlocked);
    }
    void duplicateUnlockDoesNotStartSecondRequest() {
        command("DeferUnlocks");
        KWalletBackend backend;
        backend.unlock(0);
        backend.unlock(0);
        QDBusInterface service(serviceName, servicePath,
                               "org.freedesktop.Secret.Service");
        QTRY_COMPARE(QDBusReply<int>(service.call("UnlockCalls")).value(), 1);
        command("FinishUnlocksInReverseOrder");
        QTRY_COMPARE(backend.state(), VaultBackend::Unlocked);
    }
    void deletedCollectionRejectsLateUnlockReply() {
        command("DeferUnlocks");
        KWalletBackend backend;
        backend.unlock(0);
        QDBusInterface service(serviceName, servicePath,
                               "org.freedesktop.Secret.Service");
        QTRY_COMPARE(QDBusReply<int>(service.call("UnlockCalls")).value(), 1);
        command("DeleteCollection");
        QTRY_COMPARE(backend.state(), VaultBackend::Locked);
        QSignalSpy changes(&backend, &VaultBackend::changed);
        command("FinishUnlocksInReverseOrder");
        QTest::qWait(100);
        QCOMPARE(backend.state(), VaultBackend::Locked);
        QCOMPARE(changes.count(), 0);
    }
    void explicitAccountDisablePreventsUnlock() {
        // A fresh process avoids KConfig caching another test's enabled policy.
        QProcess child;
        child.start(QCoreApplication::applicationFilePath(), {"--disabled-policy"});
        QVERIFY(child.waitForStarted());
        QVERIFY(child.waitForFinished());
        QCOMPARE(child.exitStatus(), QProcess::NormalExit);
        QCOMPARE(child.exitCode(), 0);
        QCOMPARE(child.readAllStandardOutput().trimmed(), QByteArray("ACCOUNT_DISABLE_PRESERVED"));
        QDBusInterface service(serviceName, servicePath,
                               "org.freedesktop.Secret.Service");
        QDBusReply<int> calls = service.call("UnlockCalls");
        QVERIFY(calls.isValid());
        QCOMPARE(calls.value(), 0);
    }
    void unrelatedOrUnlockedChangeDoesNotLockBackend() {
        KWalletBackend backend;
        backend.unlock(0);
        QTRY_COMPARE(backend.state(), VaultBackend::Unlocked);
        command("OtherCollectionChanged");
        command("UnlockedCollectionChanged");
        QTest::qWait(100);
        QCOMPARE(backend.state(), VaultBackend::Unlocked);
    }
    void serviceLockDismissesVisiblePasswordEditor() {
        KWalletBackend backend;
        VaultWindow window(&backend);
        backend.unlock(0);
        QTRY_COMPARE(backend.state(), VaultBackend::Unlocked);
        QTimer::singleShot(0, &window, [&] {
            auto *dialog = window.findChild<QDialog *>("credentialEditor");
            QVERIFY(dialog);
            auto *password = dialog->findChild<QLineEdit *>("password");
            password->setText("synthetic-unsaved-value");
            password->setEchoMode(QLineEdit::Normal);
            QTimer::singleShot(3000, dialog, &QDialog::reject);
            command("ExternalLock");
        });
        window.findChild<QPushButton *>("addCredential")->click();
        QCOMPARE(backend.state(), VaultBackend::Locked);
        QVERIFY(!window.findChild<QDialog *>("credentialEditor"));
        QVERIFY(!window.findChild<QPushButton *>("addCredential")->isEnabled());
    }
    void unavailableCollectionFailsClosed() {
        KWalletBackend backend;
        backend.unlock(0);
        QTRY_COMPARE(backend.state(), VaultBackend::Unlocked);
        command("LoseCollectionObject");
        QTRY_COMPARE(backend.state(), VaultBackend::Locked);
    }
    void collectionDeletionLocksBackend() {
        KWalletBackend backend;
        backend.unlock(0);
        QTRY_COMPARE(backend.state(), VaultBackend::Unlocked);
        command("DeleteCollection");
        QTRY_COMPARE_WITH_TIMEOUT(backend.state(), VaultBackend::Locked, 2000);
    }
    void serviceLossLocksBackend() {
        KWalletBackend backend;
        backend.unlock(0);
        QTRY_COMPARE(backend.state(), VaultBackend::Unlocked);
        command("LoseService");
        QTRY_COMPARE(backend.state(), VaultBackend::Locked);
    }
    void unverifiableLockDoesNotRestoreUnlockedUI() {
        KWalletBackend backend;
        QList<VaultBackend::State> states;
        connect(&backend, &VaultBackend::changed, &backend, [&] { states.append(backend.state()); });
        VaultWindow window(&backend);
        backend.unlock(0);
        QTRY_COMPARE(backend.state(), VaultBackend::Unlocked);
        QSignalSpy errors(&backend, &VaultBackend::error);
        command("LoseObjectOnNextLock");
        states.clear();
        backend.lock();
        QTRY_COMPARE(errors.count(), 1);
        QVERIFY(!states.contains(VaultBackend::Unlocked));
        QCOMPARE(backend.state(), VaultBackend::Locked);
        QVERIFY(!window.findChild<QPushButton *>("addCredential")->isEnabled());
        QVERIFY(errors.first().first().toString().contains("could not be verified"));
    }
    void confirmedLockRefusalWarnsWithoutClaimingSuccess() {
        KWalletBackend backend;
        backend.unlock(0);
        QTRY_COMPARE(backend.state(), VaultBackend::Unlocked);
        QSignalSpy errors(&backend, &VaultBackend::error);
        command("RefuseNextLock");
        backend.lock();
        QTRY_COMPARE(errors.count(), 1);
        QCOMPARE(backend.state(), VaultBackend::Unlocked);
        QVERIFY(errors.first().first().toString().contains("could not be locked"));
    }
    void lockingOneClientClearsBothWindows() {
        KWalletBackend first, second;
        VaultWindow firstWindow(&first), secondWindow(&second);
        first.unlock(0);
        QTRY_COMPARE(first.state(), VaultBackend::Unlocked);
        second.unlock(0);
        QTRY_COMPARE(second.state(), VaultBackend::Unlocked);
        first.lock();
        QTRY_COMPARE(first.state(), VaultBackend::Locked);
        QTRY_COMPARE(second.state(), VaultBackend::Locked);
        QVERIFY(!firstWindow.findChild<QPushButton *>("addCredential")->isEnabled());
        QVERIFY(!secondWindow.findChild<QPushButton *>("addCredential")->isEnabled());
    }
};

int main(int argc, char **argv)
{
    qDBusRegisterMetaType<QList<QDBusObjectPath>>();
    if (argc == 2 && QByteArray(argv[1]) == "--disabled-policy") {
        QCoreApplication app(argc, argv);
        QTemporaryDir config;
        if (!config.isValid()) return 2;
        qputenv("XDG_CONFIG_HOME", config.path().toUtf8());
        qputenv("XDG_CONFIG_DIRS", config.path().toUtf8());
        const QByteArray disabled("[Wallet]\nEnabled=false\n");
        QFile policy(config.filePath("kwalletrc"));
        if (!policy.open(QIODevice::WriteOnly) || policy.write(disabled) != disabled.size()) return 3;
        policy.close();
        KWalletBackend backend;
        QSignalSpy errors(&backend, &VaultBackend::error);
        backend.unlock(0);
        if (backend.state() != VaultBackend::Locked || errors.count() != 1
            || !errors.first().first().toString().contains("disabled")) return 4;
        if (!policy.open(QIODevice::ReadOnly) || policy.readAll() != disabled) return 5;
        puts("ACCOUNT_DISABLE_PRESERVED");
        return 0;
    }
    if (argc == 2 && QByteArray(argv[1]) == "--mock-service") {
        QCoreApplication app(argc, argv);
        MockSecrets secrets;
        MockWallet wallet;
        auto bus = QDBusConnection::sessionBus();
        const auto exports = QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals
            | QDBusConnection::ExportAllProperties;
        if (!bus.registerService(serviceName)
            || !bus.registerService("org.kde.kwalletd6")
            || !bus.registerObject(servicePath, &secrets, exports)
            || !bus.registerObject(collectionPath, &secrets.collection, exports)
            || !bus.registerObject("/modules/kwalletd6", &wallet, exports)) return 2;
        puts("READY");
        fflush(stdout);
        return app.exec();
    }
    QApplication app(argc, argv);
    app.setApplicationName("aero7-vault-backend-test");
    KWalletBackendTest test;
    return QTest::qExec(&test, argc, argv);
}
#include "KWalletBackendTest.moc"
