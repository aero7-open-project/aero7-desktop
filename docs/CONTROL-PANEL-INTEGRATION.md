# Control Panel integration

The separately maintained `linux-control-panel` package is the only Control
Panel. It installs `/usr/bin/control` and `linux-controlpanel.desktop`, with a
Windows 7-style Category View, icon views, search, deep pages, and allowlisted
bridges to real system settings.

All shell routes use this executable:

- Start side-panel Control Panel: `control`
- Devices and Printers: `control --page devices-and-printers`
- Default Programs: `control --setting default-apps`
- Searchable settings: `control --setting <catalog-key>`

The former in-tree Control Panel is not built, installed, advertised over
D-Bus, or used as a fallback. Device Manager, Computer Management, Programs
Center, and File Explorer remain separate applications.
