#!/usr/bin/env bash
set -euo pipefail
package_file="${1:?package file is required}"
marker="${XDG_STATE_HOME:-$HOME/.local/state}/aero7-desktop/migration.json"
before="$(sha256sum "$marker" | awk '{print $1}')"
sudo pacman -U --noconfirm "$package_file"
systemctl --user daemon-reload
aero7-migrate >/dev/null
after="$(sha256sum "$marker" | awk '{print $1}')"
[[ "$before" == "$after" ]] || { printf 'FAIL reinstall changed completed migration state\n' >&2; exit 1; }
/usr/lib/aero7-desktop/test-session.sh
printf 'PASS package reinstall is idempotent and preserves migration state\n'
