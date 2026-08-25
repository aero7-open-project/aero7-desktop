# Aero7 Internet Explorer

`aero7-internet-explorer` provides Aero7's permanent Windows 7-style
**Internet Explorer** shell identity while safely delegating page rendering to
a modern browser selected by the user. It does not contain, recreate, rename,
patch, or fork Internet Explorer or any installed browser.

The launcher discovers native and Flatpak browser desktop entries, stores a
per-user backend choice, supports HTTP/HTTPS/HTML defaults through the stable
wrapper desktop ID, and exposes Windows 7-style new-window and InPrivate task
actions when the selected browser advertises compatible desktop actions.

The Aero7 package-repository recipe carries the component as
`aero7-internet-explorer`. It is integrated with Start/search, taskbar pins and
jump lists, desktop shortcuts, Programs Center, and Control Panel > Default
Programs.

Configuration is stored in:

```text
~/.config/aero7/internet-explorer.conf
```

See `docs/ARCHITECTURE.md`, `docs/SECURITY.md`, `docs/TASKBAR-IDENTITY.md`, and
`docs/VM-RESULTS-2026-08-25.md` for implementation and validation details.

Aero7 is independent from Microsoft Corporation. Windows and Internet
Explorer are Microsoft trademarks. No Microsoft binaries or artwork are
included.
