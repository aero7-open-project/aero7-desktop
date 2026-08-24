# Included Components

Aero7 Desktop is a complete session package, not a second shell layered over
an unrelated visible desktop.

| Component | User-facing role |
| --- | --- |
| Aero7 Desktop session | Normal Wayland login and Safe Mode |
| Aero desktop containment | Wallpaper, desktop items, context actions, and widgets |
| Aero taskbar | Start, grouped tasks, tray, clock/date, and Show Desktop |
| SevenStart and SevenTasks | Start menu/search and taskbar behavior |
| Aero7 Control Panel | Settings, administration, and system-information routes |
| Aero7 File Explorer | Files, Libraries, Computer, Network, Recycle Bin, and dialogs |
| Aero7 Desktop Gadgets | Gallery, persistence, and nine built-in gadgets |
| Aero7 recovery | Health checks, diagnostics, same-shell restart, and safe reset |
| KWin integrations | Glass, blur, Snap, Shake, Peek, switchers, and display behavior |

## Real system backends

The familiar Aero7 interface is backed by native Linux technology:

- **KWin and Wayland** for composition, windows, effects, and displays
- **KIO and Solid** for files, devices, trash, removable media, and supported
  network locations
- **NetworkManager** for network state and connections
- **PipeWire/PulseAudio** for sound devices and volume
- **UPower and power-profiles-daemon** for battery and power information
- **pacman** for installed software and updates

## No duplicate shell surfaces

The corrected desktop uses one Aero shell implementation. Historical duplicate
taskbar, Start, tray, notification, desktop-surface, and Control Panel programs
are not installed or started in a normal session.

## Desktop Gadgets

The native Gadgets runtime includes:

- Calendar
- Clock
- CPU Meter
- Currency
- Feed Headlines
- Picture Puzzle
- Slide Show
- Weather
- Media Center

The artwork and runtime are original Aero7 implementations. Microsoft gadget
resources are not redistributed.
