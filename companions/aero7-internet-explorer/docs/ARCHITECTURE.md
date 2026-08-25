# Architecture

## Stable frontend identity

`aero7-internet-explorer.desktop` is the permanent Aero7 identity used by
Start, search, taskbar pins, desktop shortcuts, MIME associations and jump-list
actions. It never changes when the backend browser changes.

The executable and `libaero7compatlauncher` library resolve that stable identity
to a per-user backend desktop ID. Installed browser packages and their desktop
files are never renamed or modified.

## Shared component

The library owns:

- `BrowserRegistry`: dynamic native and Flatpak desktop-entry discovery;
- `BrowserConfig`: per-user selection and optional system policy;
- `DefaultApplications`: explicit HTTP, HTTPS and HTML association changes;
- `InternetExplorer`: fallback resolution, safe launch jobs, desktop shortcuts,
  Programs Center routing and privacy-preserving launch records.

The executable exposes the normal launcher, the Aero7 settings dialog, and a
JSON control surface for Control Panel. Other Aero7 components do not parse
browser desktop files independently.

## Configuration

User selection:

```text
~/.config/aero7/internet-explorer.conf
```

Optional administrator policy:

```text
/etc/aero7/internet-explorer.conf
```

Example locked policy:

```ini
[Policy]
Backend=librewolf.desktop
AllowUserChange=false
```

## Launch flow

1. Validate the configured desktop ID and reject the wrapper itself.
2. Resolve the selected installed browser.
3. When no choice exists, adopt a valid non-wrapper XDG default.
4. When exactly one browser exists, adopt it.
5. When multiple browsers exist, show the Aero7 selector.
6. When the configured browser disappeared, show the missing-browser selector.
7. When none exist, offer Programs Center.
8. Launch the backend's desktop entry with `KIO::ApplicationLauncherJob` and a
   typed list of `QUrl` values.

The Wayland `XDG_ACTIVATION_TOKEN` is forwarded to the KIO launch job. Browser
desktop actions are used for new-window and private-window commands when the
backend advertises them; unknown browsers retain normal URL launching.
