// SPDX-License-Identifier: LGPL-2.0-or-later
#include "aero7vaultpresentation.h"
#include "kbetterthankdialog.h"
#include "knewwalletdialog.h"
#include <KNewPasswordDialog>
#include <KPasswordDialog>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QSignalSpy>
#include <QTest>

using namespace Aero7VaultPresentation;

class PresentationTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void exactScope()
    {
        QVERIFY(owns(QStringLiteral("Aero7 Credentials")));
        for (const auto &other : {QString(), QStringLiteral("kdewallet"), QStringLiteral("Aero7 Credentials "),
                                  QStringLiteral("aero7 credentials"), QStringLiteral("/tmp/Aero7 Credentials")}) {
            QVERIFY(!owns(other));
            QCOMPARE(title(other), QStringLiteral("KDE Wallet Service"));
            QWidget untouched;
            untouched.setWindowTitle(QStringLiteral("Original title"));
            decorate(&untouched, other);
            QCOMPARE(untouched.windowTitle(), QStringLiteral("Original title"));
            QVERIFY(untouched.windowIcon().isNull());
        }
    }

    void callerEscaping_data()
    {
        QTest::addColumn<int>("request");
        for (int i = 0; i < 5; ++i) QTest::newRow(qPrintable(QString::number(i))) << i;
    }
    void callerEscaping()
    {
        QFETCH(int, request);
        const auto kind = static_cast<Request>(request);
        const auto anonymous = prompt(kind, QString());
        QVERIFY(!anonymous.contains(QStringLiteral("KDE")));
        QVERIFY(!anonymous.contains(QStringLiteral("application")));
        QVERIFY(!anonymous.contains(QStringLiteral("Aero7 has requested")));
        const auto supplied = prompt(kind, QStringLiteral("<img src=x>& caller"));
        QVERIFY(supplied.contains(QStringLiteral("&lt;img src=x&gt;&amp; caller")));
        QVERIFY(!supplied.contains(QStringLiteral("<img")));
    }

    void errorEscaping()
    {
        const auto text = openError(-9, QStringLiteral("<bad>&"));
        QVERIFY(text.contains(QStringLiteral("-9")));
        QVERIFY(text.contains(QStringLiteral("&lt;bad&gt;&amp;")));
        QVERIFY(!text.contains(QStringLiteral("<bad>")));
    }

    void embeddedIconAndPasswordDialogs()
    {
        QIcon::setThemeName(QStringLiteral("aero7-qa-missing-theme"));
        QVERIFY(!icon().isNull());
        QVERIFY(!icon().pixmap(48, 48).isNull());
        KPasswordDialog existing;
        KNewPasswordDialog fresh;
        for (auto *dialog : {static_cast<QWidget *>(&existing), static_cast<QWidget *>(&fresh)}) {
            decorate(dialog, QStringLiteral("Aero7 Credentials"));
            QCOMPARE(dialog->windowTitle(), QStringLiteral("Aero7 Credential Vault"));
            QVERIFY(!dialog->windowIcon().isNull());
        }
        existing.setPrompt(prompt(Request::Unlock, QString()));
        fresh.setPrompt(prompt(Request::CreatePassword, QString()));
        QVERIFY(existing.prompt().contains(QStringLiteral("keep it locked")));
        QVERIFY(fresh.prompt().contains(QStringLiteral("Choose a password")));
        QSignalSpy rejected(&existing, &QDialog::rejected);
        existing.reject();
        QCOMPARE(rejected.count(), 1);
    }

    void realSetupWizard_data()
    {
        QTest::addColumn<QString>("wallet");
        QTest::newRow("aero7") << QStringLiteral("Aero7 Credentials");
        QTest::newRow("unrelated") << QStringLiteral("kdewallet");
    }
    void realSetupWizard()
    {
        QFETCH(QString, wallet);
        KWallet::KNewWalletDialog wizard(QString(), wallet);
        auto *intro = wizard.findChild<QLabel *>(QStringLiteral("labelIntro"));
        QVERIFY(intro);
        QCOMPARE(intro->text().contains(QStringLiteral("KDE has requested")), !owns(wallet));
        if (owns(wallet)) {
            QCOMPARE(wizard.windowTitle(), QStringLiteral("Aero7 Credential Vault"));
            QVERIFY(!wizard.windowIcon().isNull());
            for (auto *label : wizard.findChildren<QLabel *>()) QVERIFY(!label->text().contains(QStringLiteral("KDE Wallet")));
        }
        // Preserve upstream's default GPG choice and both real cipher controls.
        auto *gpg = wizard.findChild<QRadioButton *>(QStringLiteral("radioGpg"));
        auto *classic = wizard.findChild<QRadioButton *>(QStringLiteral("radioBlowfish"));
        QVERIFY(gpg && classic);
        QVERIFY(gpg->isChecked());
        QVERIFY(!wizard.isBlowfish());
        classic->click();
        QVERIFY(wizard.isBlowfish());
        QSignalSpy rejected(&wizard, &QDialog::rejected);
        wizard.reject();
        QCOMPARE(rejected.count(), 1);
    }

    void permissionResults_data()
    {
        QTest::addColumn<QString>("button");
        QTest::addColumn<int>("result");
        QTest::newRow("once") << QStringLiteral("_allowOnce") << 0;
        QTest::newRow("always") << QStringLiteral("_allowAlways") << 1;
        QTest::newRow("deny") << QStringLiteral("_deny") << 2;
        QTest::newRow("deny-forever") << QStringLiteral("_denyForever") << 3;
    }
    void permissionResults()
    {
        QFETCH(QString, button);
        QFETCH(int, result);
        KBetterThanKDialog dialog;
        decorate(&dialog, QStringLiteral("Aero7 Credentials"));
        dialog.setLabel(prompt(Request::Access, QString()));
        auto *control = dialog.findChild<QPushButton *>(button);
        QVERIFY(control);
        QSignalSpy finished(&dialog, &QDialog::finished);
        control->click();
        QCOMPARE(finished.count(), 1);
        QCOMPARE(dialog.result(), result);
    }
};

QTEST_MAIN(PresentationTest)
#include "PresentationTest.moc"
