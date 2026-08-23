# Architecture

## Session boundary

`aero7-session` selects normal or safe policy, runs the idempotent migration,
sets `PLASMA_DEFAULT_SHELL=io.gitgud.wackyideas.desktop`, and hands lifecycle
control to the supported Plasma Wayland session.

```text
SDDM -> aero7-session -> startplasma-wayland -> KWin Wayland
                                      |
                                      +-> plasmashell loading AeroShell
                                          +-> Aero desktop containment
                                          +-> one Aero panel per output
                                              Start | tasks | tray | clock | Show Desktop
                                      +-> Aero7 layout/health services
```

AeroShell is the visible shell. There is no stock Plasma panel behind it and
no second native overlay taskbar, Start menu, tray, notification server, or
desktop surface. The session layout reconciler removes foreign panels and
keeps exactly one `io.gitgud.wackyideas.panel` on each output.

## Reused components

The panel, desktop containment, SevenStart, SevenTasks, Aero system tray,
clock, Show Desktop, notification applet, search integration, Aero Plasma
theme, SMod decoration, cursors, icons, and sounds remain separately versioned
upstream components. This project supplies session policy, deterministic
layout, migration, health supervision, integration tests, and the ISO-owned
wallpaper/branding assets.

## Application boundaries

The real `linux-control-panel` application (`/usr/bin/control`) owns Category
View and all settings navigation. Aero7 Dolphin owns File Explorer. Device
Manager, Computer Management, Programs Center, and other standalone programs
remain separate packages. This project does not fork or embed them.

KWin Wayland owns windows and composition. SMod owns Aero window decoration;
the installed Aero KWin effects/scripts provide blur, Snap, Shake, Peek, and
switching behavior. Network, audio, power, devices, notifications, tasks, and
application launching use the corresponding real Plasma/KDE/Linux backends
through the reused AeroShell widgets.
