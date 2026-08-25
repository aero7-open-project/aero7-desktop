# Requirements audit

This audit maps the 53 implementation sections to the final local source and
the 2026-08-25 Aero7 Wayland VM evidence.

| # | Area | Status and evidence |
| ---: | --- | --- |
| 1 | Existing project inspection | Complete; AeroShell, SevenStart, SevenTasks, Control Panel, Programs Center, package repository and VM paths were inspected. |
| 2 | Package/application name | `aero7-internet-explorer`; visible name Internet Explorer. |
| 3 | Main behavior | Shared resolver delegates to the configured detected browser. |
| 4 | Permanent identity | Stable desktop ID, name and icon used by Start, search, pin, shortcut, defaults and jump list. |
| 5 | Desktop file | Installed with `%U`, HTTP, HTTPS, XHTML and HTML MIME declarations and desktop actions. |
| 6 | Browser configuration | Per-user desktop-ID backend with validated writes. |
| 7 | Browser detection | Native, manual desktop-entry and Flatpak export paths; known and unknown valid web browsers supported. |
| 8 | Launch implementation | KIO application launch jobs with typed `QUrl` lists; no shell evaluation. |
| 9 | Default fallback | Configured backend, non-wrapper system default, one-browser adoption, selector, then no-browser fallback. |
| 10 | First launch | Multiple browsers show the Aero7 selector; one browser is adopted. |
| 11 | Control Panel | Native Windows 7-style Default Programs page using the shared JSON/settings API. |
| 12 | Default Programs | Backend selector and explicit default-browser checkbox control wrapper associations. |
| 13 | XDG defaults | HTTP, HTTPS and HTML set through `xdg-mime`; prior defaults recorded and restorable. |
| 14 | Taskbar | Permanent launcher pin and actions verified; SevenTasks aliases the selected backend inside its grouping model. |
| 15 | Launch tracking | Privacy-limited state record includes backend, mode, URL count/schemes, activation token presence and launch PIDs. |
| 16 | Existing browser | Default-handler launch successfully reused an already-running Chromium session. |
| 17 | Pinned icon | Original Aero7 Internet Explorer icon verified in the live SevenTasks taskbar. |
| 18 | Running appearance | KWin retains truthful backend identity while SevenTasks exposes Internet Explorer name, icon, launcher and grouping. |
| 19 | Jump list | New window, InPrivate, settings, shortcut and unpin actions verified live. |
| 20 | Capability abstraction | `BrowserBackend` advertises new-window and private-action support. |
| 21 | Start menu | Desktop entry and pin integrate with SevenStart; live search verified. |
| 22 | Desktop shortcut | Idempotent executable Internet Explorer shortcut verified live. |
| 23 | Search | Internet Explorer result and icon verified in the Aero7 Start search. |
| 24 | Settings UI | Standalone fallback/settings dialog and native Control Panel page use Aero7Qt/Windows 7 styling. |
| 25 | Browser changes | Firefox/Chromium changes do not modify the taskbar launcher array. |
| 26 | Missing browser | Real Firefox package removal produced the missing-backend selector. |
| 27 | No browser | Isolated no-browser environment produced a Programs Center fallback dialog. |
| 28 | Private browsing | Advertised backend actions used; URL-less Chromium actions receive a typed `%U` service field and Incognito was verified live. |
| 29 | New window | Advertised backend actions used; Firefox and Chromium multi-window behavior verified live. |
| 30 | Icon | New transparent hicolor raster set generated for Aero7; provenance recorded. |
| 31 | Branding | Independent-project/trademark boundary documented; no Microsoft binaries or artwork. |
| 32 | Browser integrity | No browser package, executable, icon or desktop file is patched or renamed. |
| 33 | Flatpak | User/system Flatpak exported desktop entries are detected and launched through desktop services. |
| 34 | Native packages | Desktop-entry discovery is package-manager independent; Firefox and Chromium Arch packages verified. |
| 35 | Wayland | Plasma 6 Wayland VM verified; no `wmctrl`, `xdotool` or X11 identity hack. |
| 36 | Window title | Backend application titles remain untouched. |
| 37 | System default detection | Valid non-wrapper HTTP default can be adopted. |
| 38 | Recursion | Wrapper desktop ID is excluded from detection, config, default adoption and backend resolution. |
| 39 | Storage | `~/.config/aero7/internet-explorer.conf`. |
| 40 | Settings API | CLI getters/setters, JSON status, defaults, settings, actions and shortcut commands implemented. |
| 41 | Errors | Missing backend, invalid selection, no browser, launch failure and MIME failure paths report actionable text. |
| 42 | Logging | State log omits full URLs, query strings, fragments and browsing history. |
| 43 | Security | Desktop-ID validation, typed arguments, wrapper rejection, limited permissions and no shell evaluation. |
| 44 | Multi-user | Configuration/default history is per user; desktop shortcut uses that user's XDG Desktop folder. |
| 45 | Policy | Optional `/etc/aero7/internet-explorer.conf` backend and user-change lock supported and tested. |
| 46 | Programs Center | `--search web browser` route verified from the no-browser dialog. |
| 47 | Reuse | Compatibility logic is isolated in `libaero7compatlauncher`; browser UI remains thin. |
| 48 | Automated tests | 16 launcher tests plus two SevenTasks/CTest checks pass, covering detection, config, policy, recursion, defaults, URL forms, local HTML, action modes and stable pin translation. |
| 49 | VM tests | Aero7/Arch/Plasma 6/Wayland with Firefox and Chromium passed; see VM results. |
| 50 | Preservation | Direct browsers and Aero7 shell remain intact; repository tests pass. |
| 51 | UI style | Aero7Qt, existing Control Panel scaffold, classic controls and Aero7 window decoration verified. |
| 52 | Desired flow | Same pin switched Firefox/Chromium; default handling and identity verified. |
| 53 | Deliverables | Files, architecture, config, detection, launch/default flows, integrations, tests and limitation are documented. |

## Taskbar implementation boundary

The alias is intentionally implemented in the Aero7 SevenTasks model, not in
KWin or the browser. KWin and external task switchers can still inspect the
truthful Firefox/Chromium identity, while the Aero7 taskbar consistently shows
Internet Explorer. This avoids global window rules, X11-only matching and
browser-profile changes while covering normal, existing-process, multi-window
and private launches in the supported shell.
