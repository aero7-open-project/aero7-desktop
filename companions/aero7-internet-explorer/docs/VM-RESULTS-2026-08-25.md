# Aero7 VM results - 2026-08-25

## Environment

- Aero7 Beta 2 installed guest
- Arch Linux x86-64, kernel 7.1.9-arch1-2
- KDE Plasma 6 session with `XDG_SESSION_TYPE=wayland`
- `aero7-internet-explorer` 0.1.0-3 local package
- `aerothemeplasma-desktop-git` 6.7.0_742.r9c2d850-35 local package
- Firefox 154.0-1
- Chromium 151.0.7922.173-1

## Results

| Check | Result |
| --- | --- |
| Local Arch package build and installation | PASS |
| Final package integrity (`pacman -Qkk`, 1,192 files total) | PASS |
| Shared status API detects Firefox and Chromium | PASS |
| Control Panel selects Firefox and Chromium | PASS |
| Firefox opens `https://aero7.miku-dayo.com` | PASS |
| Chromium opens `https://example.org` | PASS |
| Encoded URL and multiple URL capture | PASS |
| New-window and InPrivate desktop actions discovered for both browsers | PASS |
| HTTP, HTTPS and HTML defaults remain the permanent wrapper | PASS |
| `gio open` delegates a link from the default-app system to Chromium | PASS |
| Default-handler delegation while Chromium is already running | PASS |
| Same taskbar pin remains byte-for-byte unchanged across backend switch | PASS |
| Firefox window groups into the permanent Internet Explorer taskbar item | PASS |
| Existing-process Firefox launches and three-window grouping | PASS |
| Chromium window groups into the same permanent taskbar item | PASS |
| Existing-process Chromium, new-window and InPrivate grouping | PASS |
| Clicking the taskbar pin routes through the wrapper with an activation token | PASS |
| 32-frame startup trace shows no duplicate taskbar tile | PASS |
| Chromium InPrivate URL stays in Chromium instead of opening KIO/File Explorer | PASS |
| Plasma shell remains running with no SevenTasks load or QML errors | PASS |
| Start search displays Internet Explorer with the permanent icon | PASS |
| Jump list displays Open new window, InPrivate, settings and shortcut tasks | PASS |
| Desktop shortcut remains Internet Explorer and executable | PASS |
| Selected Firefox package removed, missing-backend selector shown | PASS |
| No browser available, Programs Center fallback shown | PASS |
| Programs Center receives `--search web browser` | PASS |
| Direct Firefox and Chromium application identities remain available | PASS |

The automated guest script reported:

```text
PASS permanent Start/search/taskbar desktop identity is installed
PASS shared browser discovery API is valid and recursion-safe
PASS 2 modern browser backend(s) detected
PASS encoded URL and InPrivate action reached the selected backend without shell evaluation
PASS AeroShell taskbar retains the permanent Internet Explorer pin
All available Internet Explorer compatibility checks passed.
```

## Taskbar evidence

The SevenTasks launcher configuration contained exactly:

```json
["applications:org.aero7.fileexplorer.desktop",
 "applications:linux-controlpanel.desktop",
 "applications:qterminal.desktop",
 "applications:aero7-internet-explorer.desktop"]
```

This array was identical before and after switching Chromium and Firefox.
The permanent pin, Start result, shortcut and jump list therefore remain
Internet Explorer.

The live SevenTasks adapter reported the canonical shell launcher while its
internal grouping launcher changed from `applications:firefox.desktop` to
`applications:chromium.desktop`. KWin independently reported real browser
identities (`resourceClass=firefox` and `resourceClass=Chromium`). Despite those
real identities, screenshots show a single highlighted Internet Explorer icon
for normal, existing-process, multi-window and private-browser cases. No
Firefox or Chromium taskbar icon was exposed.

The taskbar pin click changed the wrapper's last-launch record to `mode=normal`,
`backendDesktopId=chromium.desktop` and `activationTokenPresent=true`. The
Internet Explorer jump-list InPrivate action likewise recorded
`mode=private-window` without changing the stored pin.

During the expanded pass, Chromium desktop actions without a `%U` placeholder
were found to send their URL to KIO/File Explorer. The launcher now creates a
typed temporary service command with the action's existing executable and a
URL field code. Retesting opened the encoded URL in Chromium Incognito and
opened the requested new browser window without a KIO error.

## Captures

- `screenshots/control-panel-firefox.png`
- `screenshots/firefox-delegation.png`
- `screenshots/chromium-delegation.png`
- `screenshots/start-search.png`
- `screenshots/taskbar-jump-list.png`
- `screenshots/desktop-shortcut.png`
- `screenshots/removed-backend-fallback.png`
- `screenshots/no-browser-fallback.png`
- `screenshots/taskbar-firefox-grouped.png`
- `screenshots/taskbar-chromium-grouped.png`
- `screenshots/taskbar-context-menu-identity.png`
- `screenshots/chromium-inprivate-fixed.png`
