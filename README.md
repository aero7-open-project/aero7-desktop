<a id="readme-top"></a>

<div align="center">

<img src="assets/branding/aero7-logo-circle.png" width="150" alt="Aero7 logo">

# Aero7 Desktop

### A familiar Windows 7-inspired desktop for modern Linux

A complete Aero7 Wayland session with an Aero taskbar, Start menu, desktop,
Control Panel, File Explorer, notifications, recovery tools, and Desktop
Gadgets, plus a permanent Internet Explorer-compatible browser entry backed by
a maintained modern browser and current Linux/KDE infrastructure.

[![Arch Linux](https://img.shields.io/badge/Arch_Linux-supported-1793D1?logo=archlinux&logoColor=white)](https://archlinux.org/)
[![Wayland](https://img.shields.io/badge/Wayland-default-FFBC00?logo=wayland&logoColor=black)](https://wayland.freedesktop.org/)
[![KDE Plasma](https://img.shields.io/badge/KDE_Plasma-6-1D99F3?logo=kde&logoColor=white)](https://kde.org/plasma-desktop/)
[![MIT License](https://img.shields.io/badge/license-MIT-2ea44f.svg)](LICENSE)

[Features](#features) ·
[Documentation](https://github.com/aero7-open-project/aero7-desktop/wiki) ·
[Installation](#installation-and-updates) ·
[Status](#project-status) ·
[Report a bug](https://github.com/aero7-open-project/aero7-desktop/issues/new)

</div>

---

**Aero7 Desktop is an independent project and is not affiliated with or
endorsed by Microsoft Corporation. Windows is a trademark of the Microsoft
group of companies.**

> [!IMPORTANT]
> Aero7 Desktop is beta software. It is intended for Aero7 testing systems and
> should not yet be treated as a physical-hardware production certification.

## About the project

Aero7 Desktop is the user-facing desktop environment for
[Aero7](https://github.com/aero7-open-project/aero7). It recreates the familiar
layout and interaction model of Windows 7 while remaining a native Linux
desktop. KWin, KDE Frameworks, KIO, NetworkManager, PipeWire, UPower, Solid,
and standard Linux services provide the real system backends.

KDE and Plasma technology remains infrastructure underneath the session. The
normal user-facing routes open Aero7 applications such as Control Panel and
File Explorer instead of exposing a second competing desktop interface.

## Features

- Dedicated **Aero7 Desktop** Wayland session and a recoverable **Safe Mode**
- Windows 7-inspired desktop, glass taskbar, Start orb, Start menu, search,
  notification area, clock, and Show Desktop button
- Grouped taskbar windows, live previews, jump lists, Peek, Snap, Shake, and
  familiar window switching
- One synchronized Aero7 taskbar and desktop per connected display
- Aero7 Control Panel with native Linux-backed settings and system tools
- Maintained Aero7 File Explorer with Libraries, Computer, Network, Recycle
  Bin, common dialogs, and KIO-backed file operations
- Permanent Internet Explorer taskbar, Start, shortcut, and jump-list identity
  that safely delegates browsing to the selected Firefox, Chromium, or other
  supported modern browser backend
- Aero7 Desktop Gadgets gallery with Calendar, Clock, CPU Meter, Currency,
  Feed Headlines, Picture Puzzle, Slide Show, Weather, and Media Center
- Native notifications, network/audio/power status, wallpaper and theme
  defaults, migration, shell health supervision, and recovery tools
- Existing user settings are migrated non-destructively with dated backups

Unavailable functions are disabled or documented when a correct Linux backend
does not exist. Aero7 does not present decorative controls as working system
features.

## Included desktop components

| Component | Purpose |
| --- | --- |
| Aero7 session | Starts the normal or safe Wayland desktop |
| Aero desktop and taskbar | Desktop icons, Start, grouped tasks, tray, clock, and Show Desktop |
| Aero7 Control Panel | Familiar settings and system-management routes |
| Aero7 File Explorer | Files, Libraries, Computer, Network, and common dialogs |
| Aero7 Internet Explorer compatibility | Familiar browser identity, modern backend selection, default handling, taskbar grouping, new-window and InPrivate actions |
| Aero7 Desktop Gadgets | Gallery, persistence, multi-monitor placement, and nine built-ins |
| Aero7 recovery | Status, logs, same-shell restart, safe reset, and recovery UI |

The desktop package brings these maintained components together as one tested
Aero7 session. It does not install duplicate taskbars, Start menus, trays, or
settings applications. The Internet Explorer compatibility component is not a
legacy Microsoft browser engine and contains no Microsoft binaries; it keeps
the Aero7 shell identity while using an installed, updated browser for real
web content.

## Installation and updates

Aero7 Desktop is maintained as the `aero7-desktop` package in the official
[Aero7 Package Repository](https://github.com/memegeko/aero7-repo). The package
recipe is available on that repository's `beta` branch and is updated with the
rest of the Aero7 desktop stack.

After the beta package set containing Aero7 Desktop has been published, a
system with the Aero7 repository configured can install or update it with:

```bash
sudo pacman -Syu aero7-desktop
```

During a beta publication freeze, the signed pacman endpoint can temporarily
lag behind the package recipes. See
[Installation and Updates](https://github.com/aero7-open-project/aero7-desktop/wiki/Installation-and-Updates)
for repository verification, session selection, and removal guidance.

## Documentation

| Topic | Wiki page |
| --- | --- |
| Desktop, taskbar, Start, and window behavior | [Desktop User Guide](https://github.com/aero7-open-project/aero7-desktop/wiki/Desktop-User-Guide) |
| Included applications and system tools | [Included Components](https://github.com/aero7-open-project/aero7-desktop/wiki/Included-Components) |
| Displays and synchronized multi-monitor behavior | [Displays and Multi-Monitor](https://github.com/aero7-open-project/aero7-desktop/wiki/Displays-and-Multi-Monitor) |
| Safe Mode, backups, and shell repair | [Safe Mode and Recovery](https://github.com/aero7-open-project/aero7-desktop/wiki/Safe-Mode-and-Recovery) |
| Current beta limitations | [Known Issues](https://github.com/aero7-open-project/aero7-desktop/wiki/Known-Issues) |
| Common problems and useful report details | [Troubleshooting](https://github.com/aero7-open-project/aero7-desktop/wiki/Troubleshooting) |

The versioned wiki source is kept in [`wiki/`](wiki) and synchronized to the
GitHub Wiki from the `beta` branch.

## Project status

The current 0.2 beta line has been validated in an Arch/Plasma virtual machine
for package installation, clean login, Start and taskbar interaction, Control
Panel, File Explorer, Gadgets, recovery, upgrade, reinstall, uninstall, and a
nested multi-monitor session. The Internet Explorer compatibility component
was additionally tested with Firefox and Chromium for URL delegation,
new-window and private-window actions, stable taskbar grouping, and a
frame-by-frame startup check with no duplicate browser tile.

Physical GPU, USB, suspend/resume, mixed-DPI, real connector hotplug, and broad
hardware combinations still require testing. VM validation is useful release
evidence, but it is not a substitute for physical-hardware acceptance.

## Related Aero7 projects

- [Aero7](https://github.com/aero7-open-project/aero7) — operating system and ISO
- [Aero7 File Explorer](https://github.com/aero7-open-project/aero7-file-explorer) — maintained Dolphin-based file manager
- [Aero7 Control Panel](https://github.com/aero7-open-project/aero7-control-panel-) — settings and configuration
- [Aero7 Package Repository](https://github.com/memegeko/aero7-repo) — signed packages and updates

## License

Aero7 Desktop is distributed under the [MIT License](LICENSE). Maintained
dependencies, companion projects, and third-party assets retain their own
licenses; see [THIRD_PARTY.md](THIRD_PARTY.md).

No Microsoft binaries, Windows system files, or proprietary Microsoft artwork
are included.

## Legal / Trademark Notice

Aero7 Desktop is an independent open-source project and is not affiliated
with, authorized, sponsored, endorsed, or approved by Microsoft Corporation.

Microsoft and Windows are trademarks of the Microsoft group of companies. All
other trademarks are the property of their respective owners.

This project recreates interface concepts using original and freely licensed
software and artwork.

<p align="right"><a href="#readme-top">Back to top</a></p>
