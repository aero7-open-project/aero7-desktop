# Taskbar identity

## Verified stable behavior by design

The pinned taskbar item is always
`applications:aero7-internet-explorer.desktop`. Its name, icon, desktop actions
and launch command never depend on the backend. Changing from LibreWolf to
Chromium therefore does not require repinning. The same is true for Start and
desktop shortcuts.

The desktop actions provide Windows 7-style `Open new window`, `Start InPrivate
Browsing`, settings and desktop-shortcut tasks. The shared capability layer
delegates to matching actions advertised by the selected browser and falls
back to normal launch for unknown browsers.

## Running browser windows

SevenTasks now contains an Aero7 task-model adapter. It maps the permanent
Internet Explorer launcher to the selected browser only inside Plasma's task
grouping model. The model can therefore group an existing Firefox or Chromium
window into the Internet Explorer taskbar slot even when the browser reuses an
existing process. The visible roles remain Internet Explorer: name, icon,
launcher URL, application ID and jump-list service.

This does not rename, patch or wrap the browser window itself. KWin still sees
the truthful backend identity (`firefox` or `Chromium`), while SevenTasks
provides the Aero7 shell identity. Changing the selected browser updates the
internal grouping launcher through the watched configuration file without
rewriting the stored taskbar pin.

The wrapper desktop entry deliberately uses `StartupNotify=false`. Firefox or
Chromium owns the real startup feedback after delegation; advertising another
wrapper startup task would briefly create a duplicate Internet Explorer tile
beside the permanent pin. The Wayland activation token is still forwarded to
the selected backend.

Taskbar activation and open-URL requests are routed back through
`aero7-internet-explorer.desktop`, so clicking the pin still performs backend
selection, launch tracking, default handling and InPrivate/new-window
delegation. Direct changes to Firefox or Chromium profiles are unnecessary.
