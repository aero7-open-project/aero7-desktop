# Wallpaper persistence and source-only GitHub checks

## Changes

- Removed both Desktop GitHub compilation/package jobs. The remaining workflow
  validates source syntax and runs script-level regression tests. Binary builds
  and publication remain the dedicated builder VM/package server's job.
- Removed the global wallpaper application from login, layout repair and
  appearance repair. A colocated, installed Plasma script initializes only
  desktops without a configured image and preserves non-image plugins.
- Made the shell's first-use script use the same preserve-existing rule.
- Removed the 15 temporary `archive/aerothemeplasma/*` tags from Desktop.
  Their exact refs were verified in the complete offline Git bundle before
  deletion. The retired theme repository remains public and archived.

## Verification

- Source validation, local QML validation and `git diff --check`: passed.
- GitHub run `36910874424` for `8451840`: its sole `source-checks` job passed,
  with checkout, validation-tool installation and source/script tests only.
  No compiler, Arch container or package-build job ran.
- Node regression exercises both shipped scripts: existing images, separate
  per-monitor choices, slideshow, color, empty/new desktops, repeated calls
  and a changed first-login wallpaper: passed.
- Real session setup executed against command doubles: 4 tests passed.
- Existing combined test tree reconfigured, not rebuilt: 44/44 selected CTest
  tests passed. `install-profiles` was excluded because it compiles new test
  profiles; no new package was built for this fix.
- A clean temporary staging directory contains the new installed helper and
  passes all 17 required combined-install files and configuration checks.
  An initial reuse of an older staging directory exposed stale `/usr/etc`
  contents; the clean stage has no such contents.
- Disposable installed Aero7 VM: directly installed the three changed runtime
  scripts, selected a distinct custom PNG, ran layout-only reconciliation and
  appearance reconciliation, rebooted and logged in again. Live Plasma state
  retained `file:///home/test/Pictures/aero7-wallpaper-persistence.png`.
  The session setup service completed successfully after reboot; the captured
  desktop visibly retained the custom image with the taskbar loaded.

This is not a new ISO, binary release or physical-host reboot certification.
The chosen wallpaper already overwritten by older versions is not recovered
by this fix; select it once again after installing the corrected scripts.
