# Upstream maintenance

Aero7 Desktop does not vendor or fork KWin, Plasma, KScreen, KIO,
NetworkManager, PipeWire, Solid, UPower, or KDE Frameworks. Those are pinned
runtime/build dependencies and updated through the compatibility process.

| Component | Upstream / pin | Aero7 change | Licence | Update method / reason |
| --- | --- | --- | --- | --- |
| Aero7 File Explorer | `https://github.com/aero7-open-project/aero7-file-explorer`, package 25.12.3-30; forked from KDE Dolphin | Maintained Windows 7-style Explorer UI and Aero7 common dialogs; this repo adds session integration | GPL/LGPL family upstream | Rebase the maintained fork onto Dolphin releases and repeat Explorer/remote-KIO acceptance |
| SMod / Aero KWin components | Separately packaged tested revisions | Enabled subset only; broken/unavailable effects disabled | package/upstream licences | Rebuild and run KWin matrix for every KWin ABI update |
| Aero7 Desktop Gadgets 3.0 | Included at `companions/aero7-gadgets` | Native Wayland/X11 host, gallery, persistence and nine painted built-ins | MIT | Build and self-test in this repository; repeat live placement/network tests before releases |
| AeroThemePlasma-derived shell | Included at `theme/`, imported from `84c7aeff5444c73a27680370d123785acbf7c1c3` | Visible Aero7 desktop, panel, Start, tray, SDDM and native helpers | AGPL-3.0-or-later and retained KDE notices | Maintain in this repository on `testing`; repeat shell/session tests for upstream updates |
| Repository signing key | `memegeko/aero7-repo`, recorded local revision | None | authentication material | Fingerprint and SHA-256 pinned in `THIRD_PARTY.md` |

The two small Aero7 KWin scripts in this repository are original integration
code using the documented JavaScript scripting API, not forks. The File
Explorer locale overlay translates only the upstream product-name message and
falls back to the user's normal Dolphin translations for every other string.

When an upstream fix works unchanged, update the dependency instead of copying
the patch. Record new commits, licences, API changes, and VM results here and
in the compatibility manifest.
