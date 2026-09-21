# Desktop User Guide

For the exact clean-account layout, session selector, lock screen, and upgrade
behavior, start with [First Login and Defaults](First-Login-and-Defaults).
The [screenshot tour](Screenshots) shows the installed test build at 1920×1080.

## Desktop

The Aero7 desktop shows the configured wallpaper and desktop items on the
primary display. Right-clicking the desktop opens the Aero7 context menu for
view, sorting, refresh, new items, display settings, Gadgets, and
personalization. Desktop edit-mode controls are not part of the normal Aero7
experience.

## Start menu and search

Open Start with the Start orb or the Meta key. The menu provides applications,
frequent and recent items, familiar places, Control Panel, Computer, session
actions, and application search. Opening Start again closes the existing menu;
the desktop does not create multiple Start instances.

## Taskbar

Right-click empty taskbar space for window-arrangement commands, Task Manager,
Lock the taskbar, and Properties. Right-click a program tile for that program's
jump list and pinning actions. Right-click Start for its Properties/Explorer
routes. These menus are different contexts; taskbar properties do not enable
Plasma desktop edit mode.

The taskbar combines pinned applications and running windows. Windows from the
same application are grouped, with previews and jump-list actions where the
application provides them. The notification area contains application status,
network, audio, power, hidden icons, clock/date, and Show Desktop.

The permanent Internet Explorer pin opens the selected modern browser without
changing its Aero7 taskbar identity. Its jump list includes new-window and
InPrivate actions. Changing the browser backend does not require repinning, and
browser startup remains one taskbar tile rather than showing a temporary
duplicate.

## Windows

Aero7 uses KWin Wayland for real window management:

- drag a window to an edge to use Aero Snap;
- use the taskbar previews to switch grouped windows;
- hover supported taskbar previews for Peek;
- use the standard window buttons to minimize, maximize, restore, or close;
- use the familiar window switcher to move between open applications.

## Control Panel

Open Control Panel from Start or the desktop personalization and display
routes. Aero7 Control Panel presents familiar categories and applets while
using real Linux services for updates, networking, sound, power, users,
firewall, packages, and hardware information.

Some advanced settings can use an individual KDE control module as a
compatibility backend. The full KDE System Settings application is not the
normal user-facing settings interface.

## File Explorer

File Explorer provides Favorites, Libraries, Computer, Network, Recycle Bin,
common file dialogs, multiple view modes, details and preview areas, and real
file operations through KIO. Raw Linux implementation mounts are filtered from
the normal Computer presentation.

## Desktop Gadgets

Open **Gadgets** from the desktop context menu to add Calendar, Clock, CPU
Meter, Currency, Feed Headlines, Picture Puzzle, Slide Show, Weather, or Media
Center. Gadget positions and the selected display are restored at login.

Read [Desktop Gadgets](Desktop-Gadgets) for each built-in, network use, local
settings, and migration from legacy test widgets.

## Screenshots and optional features

**Meta+Shift+S** opens rectangular selection. Release to save a PNG, copy the
image, and receive an openable notification without opening the editor. See
[Screenshots and Clipboard](Screenshots-and-Clipboard).

Search Start for **Turn Aero7 features on or off** to manage supported
optional services. **Programs Center Beta** is optional, while Desktop Core is
required. See [Optional Features](Optional-Features) before changing packages.
