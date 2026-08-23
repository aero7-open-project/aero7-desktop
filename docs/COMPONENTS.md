# Components

| Component | Responsibility |
| --- | --- |
| `aero7-session` | Normal/safe AeroShell Wayland session and migration |
| `aero7-session-setup` | Exact panel order, one panel per output, pins, theme, wallpaper, SMod |
| `aero7-shell.service` | AeroShell health, same-shell restart, output/layout reconciliation |
| AeroShell desktop containment | Wallpaper, desktop icons, context actions, widgets |
| AeroShell panel | Start, grouped tasks, Aero tray, clock/date, Show Desktop |
| SevenStart / SevenTasks | Start menu/search and taskbar behavior |
| `/usr/bin/control` | Real Windows 7-style Control Panel and settings routes |
| `aero7-file-explorer` | File Explorer wrapper around the separately packaged maintained Dolphin fork |
| `companions/aero7-gadgets` / `aero7-gadgets` | Native gadget host, gallery, persistence and nine built-ins |
| `aero7-recovery-ui` / `aero7-recovery` | Diagnostics and recoverable same-shell reset |
| SMod and Aero KWin integrations | Glass decoration, blur, Snap, Shake, Peek, switchers |

The former `aero7-desktop-surface`, `aero7-taskbar`, `aero7-start`,
`aero7-tray`, `aero7-notify`, and `aero7-control-panel` implementations are
retired from the build and package. Their source is retained only as historical
implementation evidence; none is installed or started in a normal session.
The separate native gadget host replaces the former three compatibility
plasmoids and is required by the desktop package.
