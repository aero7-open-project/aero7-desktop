<div align="center">

<img src="https://raw.githubusercontent.com/aero7-open-project/aero7-desktop/beta/assets/branding/aero7-logo-circle.png" width="140" alt="Aero7 logo">

# Aero7 Desktop Wiki

User documentation for the Windows 7-inspired Aero7 Wayland desktop.

</div>

## Welcome

Aero7 Desktop brings the familiar desktop, taskbar, Start menu, window
behavior, Control Panel, File Explorer, notifications, and Desktop Gadgets
together in one native Linux session. A permanent Internet Explorer-compatible
entry preserves the Aero7 browser identity while delegating web content to an
installed modern browser.

The current release line is a beta. It is suitable for Aero7 testing and daily
evaluation, but some physical-hardware combinations still need certification.

The 21 September rebuilt online and disconnected offline test candidates both
completed clean installation, OOBE, login and exported-log validation with
Desktop `0.2.0-33`. This validates the selected VM stack; it does not publish a
final ISO or replace the physical-hardware limits in [Known Issues](Known-Issues).

[![Aero7 Desktop at native 1920×1080](images/beta2-1080p/desktop.png)](Screenshots)

The image is an arranged documentation VM. The later clean-install acceptance
is recorded separately, and the [capture tour](Screenshots) keeps the exact
package versions and visible limits of each screenshot set.

## Start here

- **[1920×1080 Screenshot Tour](Screenshots)** — new real VM captures, package
  versions, and a clear distinction from final-ISO acceptance
- **[First Login and Defaults](First-Login-and-Defaults)** — factory pins,
  Recycle Bin, login keyboard, lock screen, and fallback sessions
- **[Screenshots and Clipboard](Screenshots-and-Clipboard)** — Meta+Shift+S,
  PNG saving, image paste, notifications, and troubleshooting
- **[Desktop Gadgets](Desktop-Gadgets)** — all nine built-ins and their limits
- **[Media Player](Media-Player)** — VLC-backed Now Playing, playlist, and media defaults
- **[Optional Features](Optional-Features)** — Programs Center Beta and the
  supported optional services
- **[Installation and Updates](Installation-and-Updates)** — install from the
  Aero7 pacman repository and select the desktop session
- **[Desktop User Guide](Desktop-User-Guide)** — use the desktop, taskbar,
  Start menu, windows, and notification area
- **[Included Components](Included-Components)** — learn what is part of the
  complete Aero7 desktop
- **[Displays and Multi-Monitor](Displays-and-Multi-Monitor)** — understand
  Aero7's synchronized display behavior
- **[Safe Mode and Recovery](Safe-Mode-and-Recovery)** — recover the same Aero7
  shell without falling back to a competing desktop
- **[Known Issues](Known-Issues)** — review the exact beta limitations
- **[Troubleshooting](Troubleshooting)** — collect useful information and solve
  common session problems

## Project principles

- Aero7 owns the visible desktop experience.
- Linux and KDE services remain the real backends.
- Existing settings are migrated non-destructively and backed up.
- Unsupported features are disabled or documented instead of being simulated.
- No Microsoft binaries or proprietary Windows resources are redistributed.

For source releases, package changes, and issue tracking, return to the
[Aero7 Desktop repository](https://github.com/aero7-open-project/aero7-desktop).
