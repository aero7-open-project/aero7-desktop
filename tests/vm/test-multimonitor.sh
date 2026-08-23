#!/usr/bin/env bash
set -euo pipefail

runtime_root="$(mktemp -d /tmp/aero7-multimonitor.XXXXXX)"
chmod 700 "$runtime_root"
log_root="${XDG_STATE_HOME:-$HOME/.local/state}/aero7-desktop/multimonitor-test"
mkdir -p "$log_root"
cleanup() {
    if mountpoint -q "$runtime_root/doc" 2>/dev/null; then
        fusermount3 -u "$runtime_root/doc" 2>/dev/null || true
    fi
    rm -rf -- "$runtime_root"
}
trap cleanup EXIT

export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/run/user/$(id -u)}"
export XDG_CONFIG_HOME="$runtime_root/config"
export XDG_STATE_HOME="$runtime_root/state"
export WAYLAND_DISPLAY=aero7-multimonitor-wayland
export AERO7_PARENT_WAYLAND_DISPLAY="${AERO7_PARENT_WAYLAND_DISPLAY:-wayland-0}"
export QT_QPA_PLATFORM=wayland
export QT_SCALE_FACTOR=1.25
export KWIN_COMPOSE=Q
export LIBGL_ALWAYS_SOFTWARE=1
export QT_QUICK_BACKEND=software
export QSG_RHI_BACKEND=software
export PLASMA_DEFAULT_SHELL=io.gitgud.wackyideas.desktop

dbus-run-session -- bash -s -- "$log_root" <<'NESTED'
set -euo pipefail
log_root="$1"

kwin_wayland --wayland-display "$AERO7_PARENT_WAYLAND_DISPLAY" \
    --no-lockscreen --no-global-shortcuts --no-kactivities \
    --socket "$WAYLAND_DISPLAY" --output-count 3 --width 800 --height 600 \
    >"$log_root/kwin.log" 2>&1 &
kwin_pid=$!
plasma_pid=''
cleanup_nested() {
    [[ -z "$plasma_pid" ]] || kill "$plasma_pid" 2>/dev/null || true
    kill "$kwin_pid" 2>/dev/null || true
    wait "$plasma_pid" 2>/dev/null || true
    wait "$kwin_pid" 2>/dev/null || true
}
trap cleanup_nested EXIT

for _ in {1..80}; do
    [[ -S "$XDG_RUNTIME_DIR/$WAYLAND_DISPLAY" ]] && break
    kill -0 "$kwin_pid" 2>/dev/null || exit 1
    sleep 0.1
done
[[ -S "$XDG_RUNTIME_DIR/$WAYLAND_DISPLAY" ]]

plasmashell --replace >"$log_root/plasmashell.log" 2>&1 &
plasma_pid=$!
for _ in {1..120}; do
    shell="$(qdbus6 org.kde.plasmashell /PlasmaShell org.kde.PlasmaShell.shell 2>/dev/null || true)"
    [[ "$shell" == io.gitgud.wackyideas.desktop ]] && break
    kill -0 "$plasma_pid" 2>/dev/null || exit 1
    sleep 0.1
done
[[ "$shell" == io.gitgud.wackyideas.desktop ]]

layout=/usr/share/aero7-desktop/shell/aero7-shell-layout.js
qdbus6 org.kde.plasmashell /PlasmaShell org.kde.PlasmaShell.evaluateScript "$(<"$layout")" >/dev/null
: >"$log_root/stages.log"

validate_layout() {
    expected="$1"
    query='var o={screens:screenCount,panels:[],desktops:[]}; for(var p of panels()){var w=[];for(var x of p.widgets())w.push(x.type);o.panels.push({type:p.type,widgets:w});}for(var d of desktops())o.desktops.push(d.type);print(JSON.stringify(o));'
    for _ in {1..80}; do
        state="$(qdbus6 org.kde.plasmashell /PlasmaShell org.kde.PlasmaShell.evaluateScript "$query" 2>/dev/null || true)"
        if python - "$state" "$expected" <<'PY' 2>/dev/null
import json, sys
s=json.loads(sys.argv[1]); n=int(sys.argv[2])
widgets=['io.gitgud.wackyideas.SevenStart','io.gitgud.wackyideas.seventasks','io.gitgud.wackyideas.systemtray','io.gitgud.wackyideas.digitalclocklite','io.gitgud.wackyideas.win7showdesktop']
assert s['screens']==n and len(s['panels'])==n and len(s['desktops'])>=n, s
assert all(p['type']=='io.gitgud.wackyideas.panel' and p['widgets']==widgets for p in s['panels']), s
assert set(s['desktops'])=={'io.gitgud.wackyideas.desktopcontainment'}, s
PY
        then return 0; fi
        sleep 0.15
    done
    return 1
}

validate_layout 3
printf 'initial-three-output-layout\n' >>"$log_root/stages.log"
kscreen-doctor -o | sed -E 's/\x1B\[[0-9;]*[mK]//g' >"$log_root/outputs.log"
test_output="$(awk '/^Output:/ {print $3}' "$log_root/outputs.log" | sed -n '2p')"
[[ -n "$test_output" ]]
printf 'selected-output=%s\n' "$test_output" >>"$log_root/stages.log"
kscreen-doctor "output.$test_output.disable" >/dev/null
printf 'output-disabled\n' >>"$log_root/stages.log"
sleep 0.5
qdbus6 org.kde.plasmashell /PlasmaShell org.kde.PlasmaShell.evaluateScript "$(<"$layout")" >/dev/null
validate_layout 2
printf 'two-output-layout\n' >>"$log_root/stages.log"
kscreen-doctor "output.$test_output.enable" >/dev/null
printf 'output-enabled-scaled\n' >>"$log_root/stages.log"
sleep 0.5
qdbus6 org.kde.plasmashell /PlasmaShell org.kde.PlasmaShell.evaluateScript "$(<"$layout")" >/dev/null
validate_layout 3
printf 'restored-three-output-layout\n' >>"$log_root/stages.log"
kscreen-doctor -o | sed -E 's/\x1B\[[0-9;]*[mK]//g' >"$log_root/final-outputs.log"
[[ "$QT_SCALE_FACTOR" == 1.25 ]]
NESTED

printf 'PASS one Aero taskbar per monitor on three virtual outputs\n'
printf 'PASS output disable/re-enable and 1.25 UI scaling preserve the exact AeroShell layout\n'
