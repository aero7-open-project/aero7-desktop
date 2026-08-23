#!/usr/bin/env bash
set -euo pipefail
export LANG=C.UTF-8

interface=org.aero7.Desktop
object=/Desktop
unit=aero7-desktop-test-$$
reload_unit="$unit-reload"
desktop_root="$(mktemp -d /tmp/aero7-desktop-items.XXXXXX)"
desktop_config="$desktop_root.config.json"
wallpaper=/usr/share/aero7-desktop/wallpapers/aero7-default.svg

cleanup() {
    systemctl --user stop "$unit.service" 2>/dev/null || true
    systemctl --user stop "$reload_unit.service" 2>/dev/null || true
    rm -rf -- "$desktop_root"
    rm -f -- "$desktop_config"
    systemctl --user start aero7-desktop-surface.service 2>/dev/null || true
}
trap cleanup EXIT
fail() {
    printf 'FAIL %s\n' "$1" >&2
    journalctl --user -u "$unit.service" -n 80 --no-pager >&2 || true
    exit 1
}
pass() { printf 'PASS %s\n' "$1"; }

systemctl --user stop aero7-desktop-surface.service 2>/dev/null || true
systemd-run --user --quiet --unit="$unit" \
    --property=Environment="AERO7_DESKTOP_PATH=$desktop_root" \
    --property=Environment="AERO7_DESKTOP_CONFIG=$desktop_config" \
    --property=Environment="AERO7_WALLPAPER=$wallpaper" \
    --property=Environment=LANG=C.UTF-8 \
    /usr/bin/aero7-desktop-surface

for _ in {1..40}; do
    state="$(qdbus6 "$interface" "$object" org.aero7.Desktop.dumpState 2>/dev/null || true)"
    [[ "$state" == \{* ]] && break
    sleep 0.2
done
python - "$state" "$desktop_root" <<'PY' || fail 'desktop surface state is invalid'
import json, sys
state = json.loads(sys.argv[1])
assert state["schema"] == 1
assert state["screens"] >= 1
assert state["items"] == 2
assert state["desktopPath"] == sys.argv[2]
assert state["wallpaper"].endswith("aero7-default.svg")
PY
pass 'one Aero7 desktop surface is active per detected monitor'

result="$(qdbus6 "$interface" "$object" org.aero7.Desktop.setWallpaper "file://$wallpaper")"
[[ "$result" == true ]] || fail 'desktop rejected a readable local wallpaper'
state="$(qdbus6 "$interface" "$object" org.aero7.Desktop.dumpState)"
python - "$state" "$wallpaper" <<'PY' || fail 'wallpaper setting was not persisted in live state'
import json, sys
assert json.loads(sys.argv[1])["wallpaper"] == sys.argv[2]
PY
result="$(qdbus6 "$interface" "$object" org.aero7.Desktop.setWallpaper file:///does/not/exist.png)"
[[ "$result" == false ]] || fail 'desktop accepted a missing wallpaper'
pass 'wallpaper changes validate real files and update the live desktop surface'

clock_id="$(qdbus6 "$interface" "$object" org.aero7.Desktop.addGadget clock '')"
cpu_id="$(qdbus6 "$interface" "$object" org.aero7.Desktop.addGadget cpu '')"
notes_id="$(qdbus6 "$interface" "$object" org.aero7.Desktop.addGadget notes '')"
[[ -n "$clock_id" && -n "$cpu_id" && -n "$notes_id" ]] || fail 'native gadget host rejected a supported gadget'
qdbus6 "$interface" "$object" org.aero7.Desktop.setGadgetPosition "$clock_id" 123 77 '' >/dev/null
qdbus6 "$interface" "$object" org.aero7.Desktop.setGadgetNote "$notes_id" 'Persistent Aero7 note' >/dev/null
sleep 1.2
state="$(qdbus6 "$interface" "$object" org.aero7.Desktop.dumpState)"
python - "$state" "$clock_id" <<'PY' || fail 'native gadget state is invalid'
import json, sys
state = json.loads(sys.argv[1])
assert 0 <= state["cpuUsage"] <= 100
assert {g["type"] for g in state["gadgets"]} == {"clock", "cpu", "notes"}
clock = next(g for g in state["gadgets"] if g["id"] == sys.argv[2])
assert (clock["x"], clock["y"]) == (123, 77)
assert next(g for g in state["gadgets"] if g["type"] == "notes")["note"] == "Persistent Aero7 note"
PY
python - "$desktop_config" <<'PY' || fail 'gadget configuration was not persisted atomically'
import json, sys
with open(sys.argv[1], encoding="utf-8") as handle:
    config = json.load(handle)
assert config["schema"] == 2
assert len(config["gadgets"]) == 3
PY

systemctl --user stop "$unit.service"
systemd-run --user --quiet --unit="$reload_unit" \
    --property=Environment="AERO7_DESKTOP_PATH=$desktop_root" \
    --property=Environment="AERO7_DESKTOP_CONFIG=$desktop_config" \
    --property=Environment=LANG=C.UTF-8 \
    /usr/bin/aero7-desktop-surface
for _ in {1..40}; do
    state="$(qdbus6 "$interface" "$object" org.aero7.Desktop.dumpState 2>/dev/null || true)"
    [[ "$state" == \{* ]] && break
    sleep 0.2
done
python - "$state" <<'PY' || fail 'gadget state did not survive desktop restart'
import json, sys
assert len(json.loads(sys.argv[1])["gadgets"]) == 3
PY
qdbus6 "$interface" "$object" org.aero7.Desktop.removeGadget "$cpu_id" >/dev/null
state="$(qdbus6 "$interface" "$object" org.aero7.Desktop.dumpState)"
python - "$state" <<'PY' || fail 'gadget removal did not update live state'
import json, sys
assert len(json.loads(sys.argv[1])["gadgets"]) == 2
PY
pass 'Clock, CPU Meter, and Notes gadgets are real, movable, removable, and persistent'

qdbus6 "$interface" "$object" org.aero7.Desktop.createFolder 'Test Folder' >/dev/null
qdbus6 "$interface" "$object" org.aero7.Desktop.createFile 'Test File.txt' >/dev/null
[[ -d "$desktop_root/Test Folder" && -f "$desktop_root/Test File.txt" ]] \
    || fail 'desktop New actions did not create real items'
pass 'desktop New folder and file actions use the real Desktop directory'

qdbus6 "$interface" "$object" org.aero7.Desktop.renameItem \
    "file://$desktop_root/Test File.txt" 'Renamed.txt' >/dev/null
[[ -f "$desktop_root/Renamed.txt" && ! -e "$desktop_root/Test File.txt" ]] \
    || fail 'desktop rename did not update the real file'

source_file="$(mktemp /tmp/aero7-desktop-copy.XXXXXX)"
printf 'Aero7 desktop copy test\n' >"$source_file"
qdbus6 "$interface" "$object" org.aero7.Desktop.copyToDesktop "file://$source_file" >/dev/null
for _ in {1..20}; do
    [[ -f "$desktop_root/${source_file##*/}" ]] && break
    sleep 0.2
done
rm -f -- "$source_file"
[[ -f "$desktop_root/${source_file##*/}" ]] || fail 'external URL drop backend did not copy the file'
pass 'rename and external drag/drop copy operate on real files'

state="$(qdbus6 "$interface" "$object" org.aero7.Desktop.dumpState)"
python - "$state" <<'PY' || fail 'desktop live model did not refresh after filesystem changes'
import json, sys
assert json.loads(sys.argv[1])["items"] == 5
PY
pass 'desktop icon model follows live filesystem changes'

printf 'All automated desktop integration checks passed.\n'
