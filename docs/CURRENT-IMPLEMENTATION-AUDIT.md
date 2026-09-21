# Current implementation audit

Audit date: 2026-08-16. This is the Phase 0 baseline required by the critical
correction. It records the installed `aero7-desktop 0.2.0-1` VM before the
corrected shell architecture is packaged.

## Finding

The reported duplicate shell is real. The session launches the independent
`aero7-taskbar`, `aero7-start`, `aero7-tray`, `aero7-notify`, and
`aero7-desktop-surface` processes while `plasmashell` also loads a stock Plasma
panel. This is an overlay architecture and is not an acceptable Aero7 normal
session.

The stock panel is containment 2 in
`~/.config/plasma-org.kde.plasma.desktop-appletsrc`:

- containment plugin: `org.kde.panel`;
- application launcher: `org.kde.plasma.kickoff`;
- task manager: `org.kde.plasma.icontasks`;
- tray: `org.kde.plasma.systemtray`;
- clock: `org.kde.plasma.digitalclock`;
- Show Desktop: `org.kde.plasma.showdesktop`.

The exact cause is the health service's crash fallback. The normal session
first requests `io.gitgud.wackyideas.desktop`, whose intended layout uses the
Aero panel and applets. `io.gitgud.wackyideas.SevenStart` then triggers a
Plasma 6.7 Kicker null-containment crash. After three crashes the health
service sets `PLASMA_DEFAULT_SHELL=org.kde.plasma.desktop`, restarts
plasmashell, and writes the stock Plasma layout. The independent Aero7 overlay
services remain running, producing two taskbars and two launchers.

Core-dump evidence points to `Plasma::Applet::containment()` through
`libkickerplugin.so`; the preceding QML error is a null orb access in
`SevenStart/CompactRepresentation.qml`. Both faults are being repaired in the
isolated AeroTheme workcopy.

## Processes and visible surfaces

| Process/unit | Purpose in 0.2.0-1 | Phase 0 decision |
| --- | --- | --- |
| `kwin_wayland` | compositor and window manager | reuse unchanged |
| `plasmashell` | initially AeroShell, then stock fallback | retain only with `io.gitgud.wackyideas.desktop` |
| `aero7-desktop-surface` | second desktop surface | retire from normal session |
| `aero7-taskbar` | second taskbar | retire from normal session |
| `aero7-start` | second Start menu | retire from normal session |
| `aero7-tray` | second tray/network/audio/clock | retire from normal session |
| `aero7-notify` | second notification surface | retire from normal session once Aero tray notification tests pass |
| `aero7-shell-service` | component health and stock fallback | rewrite for AeroShell health; never generate a stock panel |

## Implementations found

Historical note (Beta 2 cleanup, 2026-09-05): the native duplicate surfaces
described below were removed from source along with their obsolete launchers,
units, and tests. This section describes the original 0.2.0-1 system, not the
current installed desktop. Recovery uses the dedicated recovery UI and session
service; the retired taskbar/Start/tray are not recovery components.

### Panels and Start menus

The installed system has two complete implementations:

1. Native 0.2.0-1 QML/C++ surfaces in `shell/taskbar`, `shell/start`, and
   `shell/tray`.
2. The existing AeroTheme/AeroShell implementation:
   `io.gitgud.wackyideas.panel`, `SevenStart`, `seventasks`, `systemtray`,
   `digitalclocklite`, and `win7showdesktop`.

The fallback adds a third, stock set of Plasma widgets. The corrected normal
session will use only implementation 2. Native binaries can remain temporarily
as a recovery implementation but must not autostart or overlap AeroShell.

### Desktop

The native 0.2.0-1 desktop draws wallpaper, icons, context actions, and three
gadgets. AeroTheme already provides
`io.gitgud.wackyideas.desktopcontainment`, while `aero7-gadgets` installs
compatibility plasmoids. The corrected normal session reuses the AeroTheme
containment and the actual Desktop directory.

### System tray and notifications

The native tray directly accesses NetworkManager, PulseAudio/PipeWire, UPower,
and StatusNotifierItem DBus services. AeroTheme already ships matching Aero
network, volume, battery, notification, and system-tray applets. The AeroTheme
applets are the corrected visible implementation; Linux services remain the
backends.

### Control Panel

The repository incorrectly contains and builds a second
`aero7-control-panel` application with its own QML, DBus service, Display,
Default Programs, Personalization, and Gadgets pages. The installed real
Control Panel is `/usr/bin/control` from `linux-control-panel`, with desktop ID
`linux-controlpanel.desktop`. It already provides the Windows 7-style category
view shown in the reference image and real Linux backends. The duplicate is to
be retired; shell links must launch or deep-link into `control`.

### Assets in 0.2.0-1

- wallpaper: a newly drawn `assets/wallpapers/aero7-default.svg`, not the real
  Aero7 background;
- icons: theme names through `QIcon::fromTheme`, but without consistently
  applying the installed `Windows 7 Aero` theme;
- cursor: not set by the session;
- sound: not set by the session;
- window decoration: SMod is configured correctly;
- Start orb and taskbar graphics: generic native QML rather than the installed
  AeroTheme assets;
- hardcoded glyphs: the shell mostly uses icon-theme names, but any symbolic
  text and fallback graphics must be reviewed against `ASSET-MAP.md`.

## KDE UI exposed by the faulty layout

The stock containment exposes Kickoff, the Plasma icon task manager, Plasma
tray popup, Plasma clock, Plasma Show Desktop, and stock panel visuals. The
session also declares `XDG_CURRENT_DESKTOP=KDE`, which is retained for backend
compatibility but must not determine the visible shell. Direct System Settings
launches must remain absent from the normal Aero7 navigation flow.

## Existing Aero7 work previously ignored or underused

- the complete AeroTheme panel layout and containment;
- SevenStart's orb, menu graphics, favorites, recent items, application model,
  search, and power actions;
- SevenTasks previews, groups, progress, media actions, and Jump Lists;
- the Aero system tray, network, volume, battery, clock, notifications, and
  Show Desktop plasmoids;
- the AeroTheme desktop containment and desktop theme graphics;
- the installed `Windows 7 Aero` icons and `aero-drop` cursors;
- the installed `Aero7` sound family;
- the real `linux-control-panel` application;
- the main Aero7 ISO background, logos, user image, and installer artwork.

## Phase 0 proof obtained so far

With the null-initialization guards applied to a local AeroTheme workcopy and
deployed temporarily to the VM, a clean AeroShell start remains active and
reports exactly one panel and one desktop. The panel is
`io.gitgud.wackyideas.panel` and contains, in order, SevenStart, SevenTasks,
the Aero system tray, Aero clock, and Aero Show Desktop. All five native
overlay services are stopped. This is a development proof; packaging,
clean-login repetition, Start interaction, multi-monitor, and visual checks
remain gates before Phase 0 passes.
