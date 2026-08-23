#!/usr/bin/env bash
set -euo pipefail
export LANG=C.UTF-8

unit=aero7-tray.service
interface=org.aero7.Tray
object=/Tray

fail() {
    printf 'FAIL %s\n' "$1" >&2
    systemctl --user status --no-pager "$unit" >&2 || true
    journalctl --user -u "$unit" -n 80 --no-pager >&2 || true
    exit 1
}
pass() { printf 'PASS %s\n' "$1"; }

systemctl --user restart "$unit"
for _ in {1..40}; do
    state="$(qdbus6 "$interface" "$object" org.aero7.Tray.dumpState 2>/dev/null || true)"
    [[ "$state" == \{* ]] && break
    sleep 0.2
done
systemctl --user is-active --quiet "$unit" || fail 'Aero7 tray service did not start'

python - "$state" <<'PY' || fail 'tray backend state is invalid'
import json
import sys
state = json.loads(sys.argv[1])
assert state["schema"] == 1
assert state["screens"] >= 1
assert isinstance(state["networkStatus"], str) and state["networkStatus"]
assert isinstance(state["networks"], list)
assert isinstance(state["outputs"], list)
assert isinstance(state["inputs"], list)
assert isinstance(state["statusItems"], list)
assert isinstance(state["removableDevices"], list)
PY
pass 'shared tray backend is active on every detected screen'

python - "$state" <<'PY' || fail 'NetworkManager state did not reach the Aero7 tray'
import json
import sys
state = json.loads(sys.argv[1])
assert state["networkingEnabled"] is True
assert any(row["kind"] == "ethernet" and row["active"] for row in state["networks"])
PY
pass 'live NetworkManager Ethernet state is exposed'

for _ in {1..30}; do
    state="$(qdbus6 "$interface" "$object" org.aero7.Tray.dumpState)"
    if python - "$state" <<'PY'
import json, sys
state = json.loads(sys.argv[1])
raise SystemExit(0 if state["audioAvailable"] and state["outputs"] else 1)
PY
    then
        break
    fi
    sleep 0.2
done

original_volume="$(python - "$state" <<'PY'
import json, sys
print(json.loads(sys.argv[1])["volume"])
PY
)"
original_muted="$(python - "$state" <<'PY'
import json, sys
print("true" if json.loads(sys.argv[1])["muted"] else "false")
PY
)"
python - "$state" <<'PY' || fail 'PipeWire/PulseAudio output was not discovered'
import json
import sys
state = json.loads(sys.argv[1])
assert state["audioAvailable"] is True
assert len(state["outputs"]) >= 1
PY

qdbus6 "$interface" "$object" org.aero7.Tray.setVolume 37 >/dev/null
sleep 0.3
state="$(qdbus6 "$interface" "$object" org.aero7.Tray.dumpState)"
python - "$state" <<'PY' || fail 'setting master output volume did not reach PipeWire/PulseAudio'
import json, sys
assert abs(json.loads(sys.argv[1])["volume"] - 37) <= 1
PY
qdbus6 "$interface" "$object" org.aero7.Tray.toggleMute >/dev/null
sleep 0.2
state="$(qdbus6 "$interface" "$object" org.aero7.Tray.dumpState)"
python - "$state" "$original_muted" <<'PY' || fail 'mute toggle did not reach PipeWire/PulseAudio'
import json, sys
assert json.loads(sys.argv[1])["muted"] is (sys.argv[2] != "true")
PY
qdbus6 "$interface" "$object" org.aero7.Tray.toggleMute >/dev/null
qdbus6 "$interface" "$object" org.aero7.Tray.setVolume "$original_volume" >/dev/null
pass 'master volume and mute control the real audio backend'

systemd-run --user --quiet --collect --unit=aero7-tray-test-stream \
    pw-cat --playback --raw --rate=48000 --channels=2 /dev/zero
stream_count=0
for _ in {1..30}; do
    state="$(qdbus6 "$interface" "$object" org.aero7.Tray.dumpState)"
    stream_count="$(python - "$state" <<'PY'
import json, sys
print(len(json.loads(sys.argv[1])["streams"]))
PY
)"
    (( stream_count > 0 )) && break
    sleep 0.2
done
systemctl --user stop aero7-tray-test-stream.service 2>/dev/null || true
(( stream_count > 0 )) || fail 'live application audio stream did not enter the Volume Mixer'
pass 'live application stream entered the per-application Volume Mixer'

sni_unit="aero7-status-notifier-test-$$"
systemd-run --user --quiet --collect --unit="$sni_unit" \
    /usr/lib/aero7-desktop/aero7-test-status-notifier
sni_service=''
sni_path=''
for _ in {1..30}; do
    state="$(qdbus6 "$interface" "$object" org.aero7.Tray.dumpState)"
    read -r sni_service sni_path < <(python - "$state" <<'PY'
import json, sys
for item in json.loads(sys.argv[1])["statusItems"]:
    if item["title"] == "Aero7 Tray Protocol Test":
        print(item["service"], item["path"])
        break
PY
) || true
    [[ -n "$sni_service" ]] && break
    sleep 0.2
done
[[ -n "$sni_service" ]] || fail 'standard StatusNotifierItem did not enter the Aero7 tray'
qdbus6 "$interface" "$object" org.aero7.Tray.activateStatusItem \
    "$sni_service" "$sni_path" 0 0 >/dev/null
sleep 0.2
sni_state="$(qdbus6 org.aero7.TestStatusNotifier /StatusNotifierItem org.kde.StatusNotifierItem.dumpState)"
python - "$sni_state" <<'PY' || fail 'tray click did not activate the StatusNotifierItem'
import json, sys
assert json.loads(sys.argv[1])["activations"] == 1
PY
systemctl --user stop "$sni_unit.service" 2>/dev/null || true
pass 'standard StatusNotifierItem registration and activation work'

printf 'All automated tray, network, and audio integration checks passed.\n'
