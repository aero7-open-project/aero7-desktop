# Optional Features and Companion Applications

The full KDE System Settings application is hidden from menus and search by
default. Open **Turn Aero7 features on or off** and select **Show KDE System
Settings application** to make it available again. Aero7 Control Panel remains
available while the KDE application is hidden.

Search Start for **Turn Aero7 features on or off**. You can also open
**Control Panel > Programs > Programs and Features** and choose the same
link. This is the feature manager's displayed name, not "Windows Features."

The [complete feature reference](https://github.com/aero7-open-project/aero7-control-panel-/wiki/Optional-Features)
lists every catalog entry, its packages, requirements, retained data, and
availability. The [Control Panel settings reference](https://github.com/aero7-open-project/aero7-control-panel-/wiki/Settings-Reference)
explains the separate searchable desktop settings.

## Core versus optional

Aero7 Desktop Core contains the session, Control Panel, and File Explorer and
cannot be disabled here. File Explorer is the maintained Dolphin fork, not an
optional mock file browser. Gadgets and browser compatibility are maintained
desktop companions.

**Programs Center Beta is optional.** It is a graphical application browser
for installing, removing, and updating software, not a dependency that should
force the desktop to be removed when it is uninstalled. On the Beta 2 install
path, a checksum-verified package is retained locally so Programs Center can
be enabled again without internet. If that cache is missing or invalid, the
manager must report it rather than silently accepting an unverified file.

Other optional entries include Parental Controls, Backup and Restore, Btrfs
System Recovery, Advanced Accessibility Services, Sync Center, Aero7 Defender,
Remote Desktop Connections, File and Printer Sharing, modem support, and Color
Management. Speech Recognition is unavailable, and the retired CardSpace
entry is not an installable feature.

**Encrypted Credential Vault** is a new optional companion, off by default,
currently under test rather than included in the frozen Beta 2 test ISOs.
Enable it in **Turn Aero7 features on or off**, sign out and back in, then open
**Control Panel > Credential Manager**. KWallet supplies encryption and password
prompts; use a strong, non-empty vault password. The Aero7 window supports adding,
editing, showing, removing and locking generic credentials. It does not import
browser passwords, autofill applications or implement Windows domain sign-in.
Disabling removes the companion and its session defaults but retains saved
vaults and existing account settings. Sign out after disabling; there is no
vault-password recovery. The next media build is intended to retain a verified
optional package for offline enabling, subject to its acceptance gate.

## Apply a change safely

1. Open the manager and inspect the current status and description.
2. Select or clear the desired feature. Read the package/service plan and any
   hardware, sign-out, or restart requirements.
3. Confirm the operation and approve the administrator prompt.
4. Wait for package/configuration verification. Do not run another package
   manager at the same time.
5. Sign out or restart only if requested, after saving your work.

Removal preserves user-created data and configuration; it is not a backup
strategy. For example, keep independent copies of backup archives and shared
files. Enabling a sharing backend does not automatically create a share, and
enabling the RDP client does not enable remote logins to your computer.

## When something goes wrong

A partial state means the package/service checks did not all pass. Read the
error before retrying. A missing package cache, unavailable repository,
missing hardware, another pacman transaction, or declined authentication each
needs a different response. Do not bypass signatures, force-remove core
packages, or delete the pacman lock without checking the active transaction.

For a report, include the feature name, displayed status, action attempted,
complete error text, package versions, and whether the machine was offline.
The [main Aero7 log guide](https://github.com/aero7-open-project/aero7/wiki/Recovery-and-Logs)
explains where installation and session diagnostics are kept.
