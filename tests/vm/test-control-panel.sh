#!/usr/bin/env bash
set -euo pipefail
export LANG=C.UTF-8
export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/run/user/$(id -u)}"
export WAYLAND_DISPLAY="${WAYLAND_DISPLAY:-wayland-0}"
export DBUS_SESSION_BUS_ADDRESS="${DBUS_SESSION_BUS_ADDRESS:-unix:path=$XDG_RUNTIME_DIR/bus}"

fail() { printf 'FAIL %s\n' "$1" >&2; exit 1; }
pass() { printf 'PASS %s\n' "$1"; }
cleanup() { pkill -KILL -x control 2>/dev/null || true; }
trap cleanup EXIT

[[ "$(command -v control)" == /usr/bin/control ]] || fail 'real Aero7 Control Panel is not installed'
[[ ! -e /usr/bin/aero7-control-panel ]] || fail 'retired duplicate Control Panel is still installed'

catalog="$(control --list-settings-json)"
python - "$catalog" <<'PY' || fail 'Control Panel catalog is incomplete'
import json, sys
rows=json.loads(sys.argv[1])
keys={row['key'] for row in rows}
required={'personalization','colors','icons','pointers','wallpaper','display','sound','sound-theme','network-status','power','accounts','date-time','region-language','default-apps','file-associations','device-actions','ease','firewall','updates','system-overview'}
assert required <= keys, sorted(required-keys)
assert len(rows) >= 60, len(rows)
assert all({'key','name','description','icon','section','keywords'} <= row.keys() for row in rows)
PY
pass 'real Control Panel exposes its searchable Windows 7-style catalog'

control --setting definitely-not-a-setting >/dev/null 2>&1 && fail 'invalid setting was accepted'
pass 'invalid Control Panel routes fail cleanly'

for arguments in '' '--page devices-and-printers' '--setting personalization'; do
    pkill -KILL -x control 2>/dev/null || true
    # shellcheck disable=SC2086
    control $arguments >/tmp/aero7-control-panel-test.log 2>&1 &
    pid=$!
    ready=false
    for _ in {1..40}; do
        if kill -0 "$pid" 2>/dev/null; then ready=true; break; fi
        sleep 0.1
    done
    [[ "$ready" == true ]] || fail "Control Panel route '$arguments' did not open"
    kill -KILL "$pid" 2>/dev/null || true
    wait "$pid" 2>/dev/null || true
done
pass 'Category View, Devices and Printers, and Personalization open in the real Control Panel'

desktop=/usr/share/applications/linux-controlpanel.desktop
grep -Fqx 'Exec=control' "$desktop" || fail 'Control Panel desktop entry does not launch control'
grep -Fqx 'Name=Control Panel' "$desktop" || fail 'Control Panel desktop entry has the wrong name'
pass 'desktop and Start-menu launch route points to the real Control Panel'
