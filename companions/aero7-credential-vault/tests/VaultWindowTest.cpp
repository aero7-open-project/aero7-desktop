// SPDX-License-Identifier: GPL-3.0-or-later
#include "VaultBackend.h"
#include "VaultWindow.h"
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QMessageBox>
#include <QPushButton>
#include <QTimer>
#include <QTreeWidget>
#include <QtTest>

class FakeVault : public VaultBackend {
public:
    State current = Locked;
    QMap<QString, Credential> data;
    bool failWrites = false;
    bool failReads = false;
    int writes = 0;
    State state() const override { return current; }
    void unlock(quintptr) override { current = Unlocked; emit changed(); }
    void lock() override { current = Locked; emit changed(); }
    QStringList entries() override { return current == Unlocked ? data.keys() : QStringList{}; }
    bool read(const QString &key, Credential &value) override {
        if (current != Unlocked || failReads || !data.contains(key)) return false;
        value = data[key]; return true;
    }
    bool write(const QString &key, const Credential &value) override {
        if (current != Unlocked || failWrites) return false;
        ++writes; data[key] = value; emit changed(); return true;
    }
    bool remove(const QString &key) override {
        if (current != Unlocked) return false;
        data.remove(key); emit changed(); return true;
    }
};

class VaultWindowTest : public QObject {
    Q_OBJECT
    static QPushButton *button(QWidget &window, const char *name) {
        return window.findChild<QPushButton *>(name);
    }
    static void completeEditor(QWidget &window, bool accept, const QString &target = "qa.example") {
        QTimer::singleShot(0, &window, [&window, accept, target] {
            auto *dialog = window.findChild<QDialog *>("credentialEditor");
            QVERIFY(dialog);
            auto *password = dialog->findChild<QLineEdit *>("password");
            QCOMPARE(password->echoMode(), QLineEdit::Password);
            auto *show = dialog->findChild<QCheckBox *>("showPassword");
            QVERIFY(show && !show->isChecked());
            show->setChecked(true);
            QCOMPARE(password->echoMode(), QLineEdit::Normal);
            show->setChecked(false);
            QCOMPARE(password->echoMode(), QLineEdit::Password);
            dialog->findChild<QLineEdit *>("target")->setText(target);
            dialog->findChild<QLineEdit *>("username")->setText("qa-user");
            password->setText("synthetic-test-value");
            if (accept) dialog->accept(); else dialog->reject();
        });
    }
private slots:
    void blankAddressAndOversizedEntryAreNotWritten() {
        FakeVault backend;
        VaultWindow window(&backend);
        backend.unlock(0);
        completeEditor(window, true, "   ");
        button(window, "addCredential")->click();
        QCOMPARE(backend.writes, 0);
        backend.data["oversize"] = {"user", QString(16385, 'x')};
        emit backend.changed();
        auto *tree = window.findChild<QTreeWidget *>("credentials");
        tree->setCurrentItem(tree->topLevelItem(0));
        button(window, "editCredential")->click();
        QCOMPARE(backend.writes, 0);
        QCOMPARE(backend.data["oversize"].password.size(), 16385);
        QVERIFY(window.findChild<QLabel *>("vaultStatus")->text().contains("exceeds"));
    }
    void lockedAndOpeningControls() {
        FakeVault backend;
        VaultWindow window(&backend);
        QVERIFY(!button(window, "addCredential")->isEnabled());
        QVERIFY(!button(window, "editCredential")->isEnabled());
        backend.current = VaultBackend::Opening; emit backend.changed();
        QVERIFY(!button(window, "unlockVault")->isEnabled());
        backend.lock();
        QVERIFY(button(window, "unlockVault")->isEnabled());
    }
    void addEditAndMask() {
        FakeVault backend;
        VaultWindow window(&backend);
        backend.unlock(0);
        completeEditor(window, true);
        button(window, "addCredential")->click();
        QCOMPARE(backend.writes, 1);
        auto *tree = window.findChild<QTreeWidget *>("credentials");
        QCOMPARE(tree->topLevelItemCount(), 1);
        QCOMPARE(tree->topLevelItem(0)->text(0), QString("qa.example"));
        QCOMPARE(tree->topLevelItem(0)->text(1), QString("qa-user"));
        QCOMPARE(tree->columnCount(), 2);
        tree->setCurrentItem(tree->topLevelItem(0));
        QTimer::singleShot(0, &window, [&] {
            auto *dialog = window.findChild<QDialog *>("credentialEditor");
            QVERIFY(dialog);
            QVERIFY(dialog->findChild<QLineEdit *>("target")->isReadOnly());
            dialog->findChild<QLineEdit *>("password")->setText("changed-test-value");
            dialog->accept();
        });
        button(window, "editCredential")->click();
        QCOMPARE(backend.data["qa.example"].password, QString("changed-test-value"));
    }
    void cancelAndDuplicatePreserveData() {
        FakeVault backend;
        VaultWindow window(&backend);
        backend.unlock(0);
        completeEditor(window, false);
        button(window, "addCredential")->click();
        QVERIFY(backend.data.isEmpty());
        backend.data["qa.example"] = {"original", "original-test-value"};
        completeEditor(window, true);
        button(window, "addCredential")->click();
        QCOMPARE(backend.writes, 0);
        QCOMPARE(backend.data["qa.example"].user, QString("original"));
        QVERIFY(window.findChild<QLabel *>("vaultStatus")->text().contains("already exists"));
    }
    void externalLockDismissesEditor() {
        FakeVault backend;
        VaultWindow window(&backend);
        backend.unlock(0);
        QTimer::singleShot(0, &window, [&] {
            auto *dialog = window.findChild<QDialog *>("credentialEditor");
            QVERIFY(dialog);
            dialog->findChild<QLineEdit *>("password")->setText("synthetic-test-value");
            backend.lock();
        });
        button(window, "addCredential")->click();
        QCOMPARE(backend.writes, 0);
        QCOMPARE(window.findChild<QTreeWidget *>("credentials")->topLevelItemCount(), 0);
    }
    void failedWriteAndUnreadableEntry() {
        FakeVault backend;
        VaultWindow window(&backend);
        backend.unlock(0);
        backend.failWrites = true;
        completeEditor(window, true);
        button(window, "addCredential")->click();
        QVERIFY(backend.data.isEmpty());
        QVERIFY(window.findChild<QLabel *>("vaultStatus")->text().contains("could not be saved"));
        backend.data["unreadable"] = {"user", "value"};
        backend.failReads = true; emit backend.changed();
        QVERIFY(window.findChild<QLabel *>("vaultStatus")->text().contains("unsupported format"));
    }
    void removalRequiresConfirmation() {
        FakeVault backend;
        backend.data["qa.example"] = {"user", "value"};
        VaultWindow window(&backend);
        backend.unlock(0);
        auto *tree = window.findChild<QTreeWidget *>("credentials");
        tree->setCurrentItem(tree->topLevelItem(0));
        for (const int response : {QMessageBox::No, QMessageBox::Yes}) {
            QTimer::singleShot(0, &window, [&window, response] {
                auto *dialog = window.findChild<QMessageBox *>();
                QVERIFY(dialog);
                QCOMPARE(dialog->defaultButton(), dialog->button(QMessageBox::No));
                dialog->done(response);
            });
            button(window, "removeCredential")->click();
            QCOMPARE(backend.data.isEmpty(), response == QMessageBox::Yes);
        }
    }
};
QTEST_MAIN(VaultWindowTest)
#include "VaultWindowTest.moc"
