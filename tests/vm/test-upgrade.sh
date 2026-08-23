#!/usr/bin/env bash
set -euo pipefail

previous="${1:-none}"
expected="${2:?expected Aero7 Desktop version is required}"
installed="$(pacman -Q aero7-desktop | awk '{print $2}' | cut -d- -f1)"
[[ "$installed" == "$expected" ]] || { printf 'FAIL expected %s after upgrade, found %s\n' "$expected" "$installed" >&2; exit 1; }
python - "${XDG_STATE_HOME:-$HOME/.local/state}/aero7-desktop/migration.json" "$expected" <<'PY'
import json, sys
with open(sys.argv[1], encoding="utf-8") as handle:
    marker = json.load(handle)
assert marker["version"] == sys.argv[2]
assert marker["schema"] >= 2
assert marker["backup"]
PY
if [[ "$previous" != none && "$previous" != "$expected" ]]; then
    printf 'PASS package upgraded from %s to %s with migration state intact\n' "$previous" "$expected"
else
    printf 'PASS package %s reinstall retained valid migration state\n' "$expected"
fi
