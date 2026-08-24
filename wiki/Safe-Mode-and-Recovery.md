# Safe Mode and Recovery

## Safe Mode

SDDM exposes two session choices:

- **Aero7 Desktop** — the normal desktop
- **Aero7 Desktop (Safe Mode)** — the same Aero7 shell with optional effects
  and extensions disabled

Safe Mode backs up the normal KWin configuration before applying its temporary
policy. It does not replace Aero7 with a stock Plasma desktop.

## Recovery application

Open the Aero7 recovery UI from the installed system when the shell layout or
startup needs repair:

```bash
aero7-recovery-ui
```

The recovery interface can show status and diagnostics, restart the same Aero7
shell, or reset managed Aero7 state after confirmation.

## Recovery commands

These commands provide the same core operations from a terminal:

```bash
aero7-recovery status
aero7-recovery logs --lines 200
aero7-recovery restart-shell
aero7-recovery reset-state
```

`reset-state` changes managed desktop state. Review the recovery UI or command
output and make sure important work is saved before proceeding.

## Backups and health supervision

Aero7 state and dated migration backups live under:

```text
~/.local/state/aero7-desktop/
```

The health service checks that the Aero shell is loaded, each output has one
Aero taskbar, and only Aero desktop containments are active. It first tries to
reconcile the layout and then restarts the same shell if necessary. It does not
create a stock Plasma taskbar as a fallback.
