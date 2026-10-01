# Desktop and theme repository consolidation

The maintained AeroThemePlasma source now lives in `theme/` in Aero7 Desktop.
Desktop, Start, taskbar, tray, splash, lock screen, SDDM, Snipping Tool and
accessibility changes use this repository's `testing` branch and issue tracker.
The previous `aero7-open-project/aerothemeplasma` repository is retired after
the combined source is tested and published. No release branch is merged by
this migration.

## Preserved source and history

The import is a non-squashed Git subtree from theme `testing` commit
`84c7aeff5444c73a27680370d123785acbf7c1c3`. Its complete commit ancestry,
source, bundled icons, screenshots, translations, notices and licences are
retained. The temporary `archive/aerothemeplasma/*` tags have been removed
from Desktop at the maintainer's request. Original branch heads and tags remain
in the retired public theme repository and verified offline Git bundles.
`theme/IMPORT.json` records provenance.
The root MIT licence does not replace the theme's AGPL or KDE notices.

The old manual installers are retained under `theme/deprecated/` for history.
They are not installed or used by Aero7. The package overlays for the accepted
login background, uncropped branding, keyboard, defaults and unified settings
search are now source files; builders do not reapply an old patch to a moving
theme tree. Recent-app proxy mapping and query coalescing remain in place.

## Builds and packages

The normal top-level CMake build includes the theme and the Aero7 session.
`AERO7_BUILD_THEME=OFF` builds the session portion on its own. The theme can
also be built directly from `theme/`, which keeps the existing package update
path compatible:

| Package | Recipe in this repository | Build source |
| --- | --- | --- |
| `aero7-desktop` | `packaging/arch/PKGBUILD` | repository root, theme disabled |
| `aerothemeplasma-desktop-git` | `packaging/arch/theme/PKGBUILD` | `theme/` |

Both recipes consume the same complete source snapshot. The signed package
repository pins both packages to a tested commit in Aero7 Desktop. This
preserves existing package names, dependencies and ownership without relying
on the retired repository for builds. Icons and sound packs retain their
separate upstream source repositories.

GitHub checks syntax and source-level regressions only: it does not compile
applications or build packages. Combined builds, staging checks and packages
belong on the dedicated builder VM; the separate signed package server remains
the distribution path. `packaging/arch/dependencies.txt` records the builder's
Arch build/test package list. Source archives omit Git data and build output.

## Publication check

The migration is published to `testing` only after the combined build,
CTest, staging, source archive and disposable-VM checks pass. QA evidence is
recorded in `docs/qa/REPOSITORY-CONSOLIDATION-2026-10-01.md`. The old repository
is kept public and archived after the new source and preserved refs are
verified on GitHub, as requested during migration. This source migration does not publish binary packages or
replace an ISO.
