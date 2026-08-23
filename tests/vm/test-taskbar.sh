#!/usr/bin/env bash
set -euo pipefail

unit=aero7-taskbar.service
interface=org.aero7.Taskbar
object=/Taskbar

fail() {
    printf 'FAIL %s\n' "$1" >&2
    systemctl --user status --no-pager "$unit" >&2 || true
    exit 1
}
pass() { printf 'PASS %s\n' "$1"; }

systemctl --user restart "$unit"
for _ in {1..30}; do
    if qdbus6 "$interface" "$object" org.aero7.Taskbar.dumpState >/dev/null 2>&1; then
        break
    fi
    sleep 0.2
done
systemctl --user is-active --quiet "$unit" || fail 'Aero7 taskbar service did not start'
pass 'Aero7 taskbar service active'

state="$(qdbus6 "$interface" "$object" org.aero7.Taskbar.dumpState)"
python - "$state" <<'PY' || fail 'taskbar state is invalid'
import json
import sys
state = json.loads(sys.argv[1])
assert state["schema"] == 1
assert state["screens"] >= 1
assert state["launchers"] == len(state["pins"])
assert state["launchers"] >= 1
assert isinstance(state["items"], list)
PY
pass 'one shared task model serves every detected screen'

jump_row="$(python - "$state" <<'PY'
import json, sys
for item in json.loads(sys.argv[1])["items"]:
    if item["jumpListEntries"] > 0:
        print(item["row"])
        break
PY
)"
[[ -n "$jump_row" ]] || fail 'no standard desktop action was exposed as a Jump List task'
jump_before="$(python - "$state" "$jump_row" <<'PY'
import json, sys
print(json.loads(sys.argv[1])["items"][int(sys.argv[2])]["jumpListEntries"])
PY
)"
qdbus6 "$interface" "$object" org.aero7.Taskbar.toggleJumpPin \
    "$jump_row" file:///tmp/aero7-jump-list-test.txt >/dev/null
state="$(qdbus6 "$interface" "$object" org.aero7.Taskbar.dumpState)"
python - "$state" "$jump_row" "$jump_before" <<'PY' || fail 'pinned Jump List item was not persisted'
import json, sys
item = json.loads(sys.argv[1])["items"][int(sys.argv[2])]
assert item["jumpListEntries"] == int(sys.argv[3]) + 1
PY
qdbus6 "$interface" "$object" org.aero7.Taskbar.toggleJumpPin \
    "$jump_row" file:///tmp/aero7-jump-list-test.txt >/dev/null
pass 'desktop actions and persistent pinned items feed the Jump List'

qdbus6 "$interface" "$object" org.aero7.Taskbar.pinLauncher \
    applications:org.kde.kcalc.desktop >/dev/null
state="$(qdbus6 "$interface" "$object" org.aero7.Taskbar.dumpState)"
python - "$state" <<'PY' || fail 'drag-to-pin backend did not accept an application launcher URL'
import json
import sys
assert any("kcalc.desktop" in pin for pin in json.loads(sys.argv[1])["pins"])
PY
pass 'launcher URL pin backend updated synchronized state'

systemd-run --user --quiet --collect --unit=aero7-taskbar-test-window \
    kcalc

row=''
for _ in {1..40}; do
    state="$(qdbus6 "$interface" "$object" org.aero7.Taskbar.dumpState)"
    row="$(python - "$state" <<'PY'
import json
import sys
for item in json.loads(sys.argv[1])["items"]:
    if item["window"] and ("kcalc" in item["appId"].lower() or "KCalc" in item["display"]):
        print(item["row"])
        break
PY
)"
    [[ -n "$row" ]] && break
    sleep 0.25
done
[[ -n "$row" ]] || fail 'live KCalc window did not enter the Wayland task model'
pass 'live Wayland application entered task model'

systemd-run --user --quiet --collect --unit=aero7-taskbar-test-window-two \
    kcalc
for _ in {1..40}; do
    state="$(qdbus6 "$interface" "$object" org.aero7.Taskbar.dumpState)"
    children="$(python - "$state" "$row" <<'PY'
import json
import sys
item = json.loads(sys.argv[1])["items"][int(sys.argv[2])]
print(item["children"])
PY
)"
    (( children >= 2 )) && break
    sleep 0.25
done
(( children >= 2 )) || fail 'two KCalc windows were not grouped in one task button'
qdbus6 "$interface" "$object" org.aero7.Taskbar.activateChild "$row" 0 >/dev/null
pass 'multiple real windows grouped with child activation support'

qdbus6 "$interface" "$object" org.aero7.Taskbar.togglePin "$row" >/dev/null
state="$(qdbus6 "$interface" "$object" org.aero7.Taskbar.dumpState)"
python - "$state" <<'PY' || fail 'unpin operation did not update synchronized state'
import json
import sys
assert not any("kcalc.desktop" in pin for pin in json.loads(sys.argv[1])["pins"])
PY
pass 'unpin operation updated synchronized state'

qdbus6 "$interface" "$object" org.aero7.Taskbar.activate "$row" >/dev/null
qdbus6 "$interface" "$object" org.aero7.Taskbar.togglePin "$row" >/dev/null || true
state="$(qdbus6 "$interface" "$object" org.aero7.Taskbar.dumpState)"
python - "$state" <<'PY' || fail 'pin operation was not reflected in shared state'
import json
import sys
state = json.loads(sys.argv[1])
assert any("kcalc.desktop" in pin for pin in state["pins"])
PY
pass 'pin operation updated shared state'

systemctl --user restart "$unit"
for _ in {1..30}; do
    state="$(qdbus6 "$interface" "$object" org.aero7.Taskbar.dumpState 2>/dev/null || true)"
    [[ "$state" == \{* ]] && break
    sleep 0.2
done
python - "$state" <<'PY' || fail 'pin did not survive taskbar restart'
import json
import sys
assert any("kcalc.desktop" in pin for pin in json.loads(sys.argv[1])["pins"])
PY
pass 'pins survived supervised taskbar restart'

initial_showing="$(python - "$state" <<'PY'
import json
import sys
print("true" if json.loads(sys.argv[1])["showingDesktop"] else "false")
PY
)"
qdbus6 "$interface" "$object" org.aero7.Taskbar.beginDesktopPeek >/dev/null
sleep 0.2
state="$(qdbus6 "$interface" "$object" org.aero7.Taskbar.dumpState)"
python - "$state" <<'PY' || fail 'Aero Peek did not temporarily reveal the desktop'
import json
import sys
state = json.loads(sys.argv[1])
assert state["desktopPeek"] is True
assert state["showingDesktop"] is True
PY
qdbus6 "$interface" "$object" org.aero7.Taskbar.endDesktopPeek >/dev/null
sleep 0.2
state="$(qdbus6 "$interface" "$object" org.aero7.Taskbar.dumpState)"
python - "$state" "$initial_showing" <<'PY' || fail 'Aero Peek did not restore the prior desktop state'
import json
import sys
state = json.loads(sys.argv[1])
assert state["desktopPeek"] is False
assert state["showingDesktop"] is (sys.argv[2] == "true")
PY
pass 'Show Desktop hover Peek restored the previous KWin state'

qdbus6 "$interface" "$object" org.aero7.Taskbar.toggleShowingDesktop >/dev/null
sleep 0.2
state="$(qdbus6 "$interface" "$object" org.aero7.Taskbar.dumpState)"
python - "$state" "$initial_showing" <<'PY' || fail 'Show Desktop did not reach KWin'
import json
import sys
assert json.loads(sys.argv[1])["showingDesktop"] is (sys.argv[2] != "true")
PY
qdbus6 "$interface" "$object" org.aero7.Taskbar.toggleShowingDesktop >/dev/null
pass 'Show Desktop toggled through KWin'

systemctl --user stop aero7-taskbar-test-window.service 2>/dev/null || true
systemctl --user stop aero7-taskbar-test-window-two.service 2>/dev/null || true

state="$(qdbus6 "$interface" "$object" org.aero7.Taskbar.dumpState)"
row="$(python - "$state" <<'PY'
import json
import sys
for item in json.loads(sys.argv[1])["items"]:
    if "kcalc.desktop" in item["launcherUrl"]:
        print(item["row"])
        break
PY
)"
[[ -z "$row" ]] || qdbus6 "$interface" "$object" org.aero7.Taskbar.togglePin "$row" >/dev/null
printf 'All automated taskbar integration checks passed.\n'
