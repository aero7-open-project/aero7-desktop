// SPDX-License-Identifier: GPL-3.0-or-later
#include "KWalletBackend.h"
#include "VaultWindow.h"
#include <Aero7Qt/stylesheet.h>
#include <QApplication>
#include <QCommandLineParser>
#include <QIcon>
#ifdef __linux__
#include <sys/prctl.h>
#endif

int main(int argc, char **argv)
{
#ifdef __linux__
    // Like the authentication agent, do not expose in-memory secrets to ptrace.
    if (prctl(PR_SET_DUMPABLE, 0) != 0) return 1;
#endif
    QApplication app(argc, argv);
    app.setApplicationName("aero7-credential-vault");
    app.setApplicationDisplayName("Credential Manager");
    app.setOrganizationName("Aero7");
    app.setApplicationVersion("0.1.0");
    app.setDesktopFileName("aero7-credential-vault");
    app.setWindowIcon(QIcon(":/aero7/icons/credential-vault.png"));
    Aero7::applyApplicationStyle(&app);
    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();
    parser.process(app);
    KWalletBackend backend;
    VaultWindow window(&backend);
    window.show();
    return app.exec();
}
