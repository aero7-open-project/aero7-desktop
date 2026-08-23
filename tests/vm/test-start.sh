#!/usr/bin/env bash
set -euo pipefail

unit=aero7-start.service
interface=org.aero7.Start
object=/Start

fail() {
    printf 'FAIL %s\n' "$1" >&2
    systemctl --user status --no-pager "$unit" >&2 || true
    journalctl --user -u "$unit" -n 60 --no-pager >&2 || true
    exit 1
}
pass() { printf 'PASS %s\n' "$1"; }

systemctl --user restart aero7-taskbar.service "$unit"
for _ in {1..40}; do
    if qdbus6 "$interface" "$object" org.aero7.Start.dumpState >/dev/null 2>&1; then
        break
    fi
    sleep 0.2
done
systemctl --user is-active --quiet "$unit" || fail 'Aero7 Start service did not start'
pass 'Aero7 Start service active'

state="$(qdbus6 "$interface" "$object" org.aero7.Start.dumpState)"
python - "$state" <<'PY' || fail 'application catalog is invalid or exposes KDE System Settings'
import json
import sys
state = json.loads(sys.argv[1])
assert state["schema"] == 1
apps = state["applications"]
assert apps["total"] > 10
for item in apps["items"]:
    assert item["storageId"] not in {"systemsettings.desktop", "org.kde.systemsettings.desktop"}
    assert item["name"].casefold() != "system settings"
PY
pass 'real application catalog loaded with System Settings hidden'

qdbus6 "$interface" "$object" org.aero7.Start.setSearchQuery 'System Settings' >/dev/null
state="$(qdbus6 "$interface" "$object" org.aero7.Start.dumpState)"
python - "$state" <<'PY' || fail 'KDE System Settings remained searchable in Aero7 Start'
import json
import sys
assert json.loads(sys.argv[1])["applications"]["items"] == []
PY
pass 'KDE System Settings is absent from Start search'

qdbus6 "$interface" "$object" org.aero7.Start.setSearchQuery kcalc >/dev/null
state="$(qdbus6 "$interface" "$object" org.aero7.Start.dumpState)"
storage_id="$(python - "$state" <<'PY'
import json
import sys
for item in json.loads(sys.argv[1])["applications"]["items"]:
    if "kcalc" in item["storageId"].casefold() or "kcalc" in item["name"].casefold():
        print(item["storageId"])
        break
PY
)"
[[ -n "$storage_id" ]] || fail 'search did not return KCalc from the real application catalog'
pass 'search filters the real application catalog'

screen="$(qdbus6 org.kde.KWin /KWin org.kde.KWin.activeOutputName 2>/dev/null || true)"
qdbus6 "$interface" "$object" org.aero7.Start.toggleOnScreen "$screen" >/dev/null
sleep 0.25
state="$(qdbus6 "$interface" "$object" org.aero7.Start.dumpState)"
python - "$state" "$screen" <<'PY' || fail 'Start menu did not open on the requested monitor'
import json
import sys
state = json.loads(sys.argv[1])
assert state["visible"] is True
if sys.argv[2]:
    assert state["screen"] == sys.argv[2]
PY
qdbus6 "$interface" "$object" org.aero7.Start.toggleOnScreen "$screen" >/dev/null
sleep 0.15
state="$(qdbus6 "$interface" "$object" org.aero7.Start.dumpState)"
python - "$state" <<'PY' || fail 'second Start activation did not close the menu'
import json
import sys
assert json.loads(sys.argv[1])["visible"] is False
PY
pass 'Start menu toggles on the requested monitor'

qdbus6 "$interface" "$object" org.aero7.Start.launchStorageId "$storage_id" >/dev/null
row=''
for _ in {1..40}; do
    taskbar_state="$(qdbus6 org.aero7.Taskbar /Taskbar org.aero7.Taskbar.dumpState 2>/dev/null || true)"
    row="$(python - "$taskbar_state" <<'PY' 2>/dev/null || true
import json
import sys
for item in json.loads(sys.argv[1])["items"]:
    if item["window"] and ("kcalc" in item["appId"].casefold() or "kcalc" in item["display"].casefold()):
        print(item["row"])
        break
PY
)"
    [[ -n "$row" ]] && break
    sleep 0.25
done
[[ -n "$row" ]] || fail 'launching KCalc did not create a real taskbar window'
qdbus6 org.aero7.Taskbar /Taskbar org.aero7.Taskbar.close "$row" >/dev/null
pass 'application launch reached the live taskbar model'

qdbus6 "$interface" "$object" org.aero7.Start.toggle >/dev/null
sleep 0.15
state="$(qdbus6 "$interface" "$object" org.aero7.Start.dumpState)"
python - "$state" <<'PY' || fail 'zero-argument Meta shortcut endpoint did not toggle Start'
import json
import sys
assert json.loads(sys.argv[1])["visible"] is True
PY
qdbus6 "$interface" "$object" org.aero7.Start.hide >/dev/null
grep -Fqx 'Meta=org.aero7.Start,/Start,org.aero7.Start,toggle' /etc/xdg/aero7-desktop/kwinrc \
    || fail 'Meta-only shortcut default is absent'
pass 'Meta-only shortcut endpoint and default are installed'

printf 'All automated Start menu integration checks passed.\n'
