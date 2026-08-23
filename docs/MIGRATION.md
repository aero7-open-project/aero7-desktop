# Migration

`aero7-migrate` is versioned, idempotent, and non-destructive. Before the first
change for a schema it creates a timestamped backup below
`~/.local/state/aero7-desktop/backups/`, writes files through temporary paths,
and commits an atomic migration marker. Re-running the same schema performs no
additional transformation.

Current schema changes establish Aero7 configuration, enable only tested KWin
effects/scripts in normal mode, disable unsupported SMod effects, and preserve
user taskbar/Start/desktop state. Existing pins and recent items are not reset
to packaged defaults during an upgrade. Safe mode has an independent exact
KWin backup marker and does not masquerade as a migration.

Tests cover a first migration, repeated migration, upgrade from an older
schema, malformed input, invalid paths, backup creation, and package
upgrade/reinstall. A failed migration exits nonzero, retains the previous
configuration and backup, records a diagnostic, and leaves the stock Plasma
session available for recovery.

Do not delete old backups automatically. A future schema must add its own
transform and test fixture; editing an already released transform would make
the marker lie about the user's state.
