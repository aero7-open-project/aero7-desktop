# Installation and Updates

## Official package

Aero7 Desktop is maintained as `aero7-desktop` in the official
[Aero7 Package Repository](https://github.com/memegeko/aero7-repo). Its package
recipe is on the repository's `beta` branch and is updated with the rest of
the Aero7 desktop stack.

When the beta package set containing Aero7 Desktop is published, install or
update it with:

```bash
sudo pacman -Syu aero7-desktop
```

The package depends on the maintained Aero7 File Explorer, Desktop Gadgets,
Control Panel, Device Manager, Computer Management, AeroTheme desktop, and the
Linux/KDE services needed by the session.

> **Beta note:** the signed pacman endpoint can temporarily lag behind the
> package recipes while a package set is frozen for testing. If pacman reports
> that `aero7-desktop` cannot be found, check the package repository's current
> beta publication status before changing repository security settings.

## Start the desktop

1. Finish the package update and log out.
2. In SDDM, choose **Aero7 Desktop** from the session menu.
3. Sign in normally.

Choose **Aero7 Desktop (Safe Mode)** only when troubleshooting effects,
extensions, layout startup, or a failed normal login.

## Updating

Use the normal Aero7 system update:

```bash
sudo pacman -Syu
```

Desktop migrations are versioned and non-destructive. Before changing managed
configuration, Aero7 writes dated backups under
`~/.local/state/aero7-desktop/backups/`.

## Repository safety

The official repository uses signed packages. Do not work around a signature
or database error by disabling signature checks, using `TrustAll`, or setting
`SigLevel = Never`.

The expected Aero7 repository signing-key fingerprint is:

```text
72C79ABBBBE96446DD3324042694BFE1090F4FD6
```

## Removing the desktop

To remove only the session integration package:

```bash
sudo pacman -R aero7-desktop
```

Review pacman's proposed transaction before accepting it. Companion Aero7
applications may remain installed because they are maintained as separate
packages and can be used or removed independently.
