# Recovery

SDDM exposes **Aero7 Desktop** and **Aero7 Desktop (Safe Mode)**. Safe mode
backs up the normal KWin configuration, disables optional effects, and starts
the same AeroShell package.

```bash
aero7-recovery status
aero7-recovery logs --lines 200
aero7-recovery restart-shell
aero7-recovery reset-state
aero7-recovery-ui
```

State and dated backups live under `~/.local/state/aero7-desktop/`. The health
service validates that AeroShell is loaded, that there is one Aero panel per
output, and that only Aero desktop containments exist. It first reconciles the
layout and, if needed, restarts the same AeroShell package. A stock Plasma panel
is never activated as a normal-session fallback.

The setup service also stops legacy overlay units during an in-place upgrade.
Those binaries and unit files are absent from the corrected package.
