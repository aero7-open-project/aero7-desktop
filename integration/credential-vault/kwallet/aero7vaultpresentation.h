// SPDX-License-Identifier: LGPL-2.0-or-later
#pragma once

#include <KLocalizedString>
#include <QIcon>
#include <QWidget>

// Presentation only. Never use this name check to authorize a caller or alter
// wallet storage, encryption, password validation, or Secret Service replies.
namespace Aero7VaultPresentation
{
inline bool owns(const QString &wallet)
{
    return wallet == QStringLiteral("Aero7 Credentials");
}

inline QString title(const QString &wallet)
{
    return owns(wallet) ? i18nd("aero7-credential-vault", "Aero7 Credential Vault") : i18n("KDE Wallet Service");
}

inline QIcon icon()
{
    // Embedded, byte-for-byte copy of the established AeroThemePlasma pack icon.
    return QIcon(QStringLiteral(":/aero7-vault/credential-vault.png"));
}

inline void decorate(QWidget *dialog, const QString &wallet)
{
    if (owns(wallet)) {
        dialog->setWindowTitle(title(wallet));
        dialog->setWindowIcon(icon());
    }
}

enum class Request { Unlock, CreatePassword, CreateFormat, Access, ChangePassword };

inline QString prompt(Request request, const QString &appId)
{
    // An empty app ID is not proof that Aero7 requested access. Do not replace
    // it with a trusted-looking caller name. Escape all supplied display text.
    QString caller;
    if (!appId.isEmpty()) {
        caller = i18nd("aero7-credential-vault", "The application '<b>%1</b>' has requested access to your credential vault.<br/><br/>",
                       appId.toHtmlEscaped());
    }
    QString instruction;
    switch (request) {
    case Request::Unlock:
        instruction = i18nd("aero7-credential-vault", "Enter the password for your Aero7 credential vault, or cancel to keep it locked.");
        break;
    case Request::CreatePassword:
        instruction = i18nd("aero7-credential-vault", "Choose a password for your new Aero7 credential vault, or cancel to deny the request.");
        break;
    case Request::CreateFormat:
        instruction = i18nd("aero7-credential-vault", "Choose the encryption type for your new Aero7 credential vault, or cancel to deny the request.");
        break;
    case Request::Access:
        instruction = i18nd("aero7-credential-vault", "Allow access to your open Aero7 credential vault?");
        break;
    case Request::ChangePassword:
        instruction = i18nd("aero7-credential-vault", "Choose a new password for your Aero7 credential vault.");
        break;
    }
    return QStringLiteral("<qt>") + caller + instruction + QStringLiteral("</qt>");
}

inline QString openError(int code, const QString &description)
{
    return i18nd("aero7-credential-vault", "<qt>Unable to unlock your Aero7 credential vault. Please try again.<br/>(Error code %1: %2)</qt>",
                 code, description.toHtmlEscaped());
}
}
