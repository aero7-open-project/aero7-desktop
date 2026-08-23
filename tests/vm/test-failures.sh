#!/usr/bin/env bash
set -euo pipefail
export LANG=C.UTF-8
export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/run/user/$(id -u)}"
export WAYLAND_DISPLAY="${WAYLAND_DISPLAY:-wayland-0}"
export DBUS_SESSION_BUS_ADDRESS="${DBUS_SESSION_BUS_ADDRESS:-unix:path=$XDG_RUNTIME_DIR/bus}"

fail() { printf 'FAIL %s\n' "$1" >&2; exit 1; }
pass() { printf 'PASS %s\n' "$1"; }
temporary_root="$(mktemp -d /tmp/aero7-failures.XXXXXX)"
trap 'rm -rf -- "$temporary_root"' EXIT

# Inject the failure Phase 0 is specifically intended to prevent: a stock
# Plasma panel alongside the Aero panel. The health service must delete it.
qdbus6 org.kde.plasmashell /PlasmaShell org.kde.PlasmaShell.evaluateScript \
    'var p=new Panel("org.kde.panel");p.location="top";print(p.id);' >/dev/null
count="$(qdbus6 org.kde.plasmashell /PlasmaShell org.kde.PlasmaShell.evaluateScript 'print(panels().length);')"
[[ "$count" -ge 2 ]] || fail 'could not inject a foreign test panel'
for _ in {1..100}; do
    state="$(qdbus6 org.kde.plasmashell /PlasmaShell org.kde.PlasmaShell.evaluateScript 'var p=panels();print(JSON.stringify({count:p.length,types:p.map(function(x){return x.type;})}));')"
    if python - "$state" <<'PY' 2>/dev/null
import json, sys
s=json.loads(sys.argv[1]); assert s['count']==1 and s['types']==['io.gitgud.wackyideas.panel']
PY
    then break; fi
    sleep 0.25
done
python - "$state" <<'PY' || fail 'health service left a duplicate stock panel visible'
import json, sys
s=json.loads(sys.argv[1]); assert s['count']==1 and s['types']==['io.gitgud.wackyideas.panel']
PY
pass 'foreign stock panel injection was reconciled back to one Aero panel'

control --setting definitely-not-an-aero-setting >"$temporary_root/control.out" 2>"$temporary_root/control.err" \
    && fail 'Control Panel accepted an invalid deep link'
grep -Fq 'Unknown Aero7 setting' "$temporary_root/control.err" || fail 'Control Panel failure had no diagnostic'
pass 'invalid Control Panel deep links fail safely'

touch "$temporary_root/not-a-directory"
if AERO7_CONFIG_HOME="$temporary_root/not-a-directory" AERO7_STATE_HOME="$temporary_root/migration-state" \
    aero7-migrate >"$temporary_root/migrate.out" 2>"$temporary_root/migrate.err"; then
    fail 'migration unexpectedly succeeded with an invalid configuration path'
fi
grep -Fq 'aero7-migrate:' "$temporary_root/migrate.err" || fail 'failed migration had no diagnostic'
pass 'invalid migration path fails cleanly with a diagnostic'

for process in aero7-taskbar aero7-start aero7-tray aero7-notify aero7-desktop-surface; do
    pgrep -x "$process" >/dev/null 2>&1 && fail "retired duplicate $process appeared"
done
pass 'failure handling never starts a retired overlay component'
