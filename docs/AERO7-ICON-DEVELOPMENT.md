# Aero7 Icon Development Policy

## Ownership rule

Application identity, window chrome, navigation, toolbar, category, status,
dialog, context-menu, tray, notification, and application-generated content
icons belong to Aero7. They must resolve from embedded resources or from files
owned by the same package. Never resolve those icons from the active desktop
theme.

Theme lookup is permitted only for provider- or user-owned content: MIME and
file icons, user folders and chosen locations, mounted devices, third-party
applications, browser backends, service-menu providers, StatusNotifierItem
providers, and external notification senders. Every source-level theme lookup
must be present in the repository's policy-test allowlist with its reason.

## Approved source pack

Use the Aero7-packaged `aerothemeplasma-icons` source at commit
`96950b8028a5d960cb683280fe5f1d9e33e6b8a2`. Copy only the assets an
application needs and embed those copies in its build. Do not replace them with
new procedural artwork. Do not make runtime lookup depend on the user's
installed theme or on `/usr/share/icons/Windows 7 Aero`.

Keep the upstream `LICENSE` and `README.md` with every imported set. Upstream
declares AGPL-3.0-or-later; its README also contains attribution, trademark,
and original-asset ownership notices which must not be removed. Record the
source filename, pinned commit, whether pixels were modified, and any rename or
resize in the component's provenance document.

## Repository layout and Qt resources

Use these locations where practical:

```text
assets/icons/app/aero7-application.png
assets/icons/pack/<size>/<upstream-name>.png
assets/icons/LICENSES.md
```

Embed application chrome under `:/aero7/pack/<size>/<name>.png` and stable
application identities under `:/aero7/icons/app/`. Add every available pack
size rather than scaling a single image when the pack supplies multiple sizes.
Test at 16, 22, 24, 32, 48, 64, 128, and 256 pixels.

Launcher install rules should point directly at the corresponding native-size
pack file and rename it to the public `aero7-*` identity during installation.
Do not generate replacement small-size art when the pack already supplies it.

## Public names and packaging

Public application and desktop-action names must begin with `aero7-`, for
example `aero7-control-panel`, `aero7-device-manager`, and
`aero7-file-explorer-view-details`. Install raster launchers in the matching
hicolor size directory through CMake/package ownership. Do not create files in
`/usr/share` at runtime. Package installs may refresh desktop/icon caches;
applications must not rebuild them on every launch.

## Failure behavior and tests

Missing resources must log `[Aero7 Icons] Missing ...`, then use another
embedded pack icon. Never fall back silently to the active theme. Each GUI
repository must include a pixel-invariance test across multiple theme names, a
missing-resource test, and a static allowlist test for theme lookups.
