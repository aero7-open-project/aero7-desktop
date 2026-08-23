# Compatibility policy

The authoritative record is
[`compatibility/desktop-stack.json`](../compatibility/desktop-stack.json). It
records exact package versions and separates automated VM results from tests
that require physical hardware.

A Plasma/KWin/Qt update is not approved merely because the project compiles.
It must pass, in order: source/static/QML tests, Arch package build, clean VM
installation, normal login and reboot, safe-mode login and exact config
restore, native shell matrix, nested multi-monitor matrix, Control Panel and
KWin checks, crash recovery, package lifecycle, and a compatibility-manifest
update. Tests which are unsupported by the environment are recorded as
`BLOCKED` or `NOT_RUN`, never converted to a pass.

The current VM baseline is Plasma Workspace 6.7.4-1, KWin 6.7.4-5, KDE
Frameworks 6.28.0-1, Qt 6.11.1-1, and Wayland 1.26.0-1. The rolling Arch
repositories may already contain newer packages; they are not automatically
approved for Aero7 by that fact.
