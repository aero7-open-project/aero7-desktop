#!/usr/bin/env bash
set -euo pipefail

expectation="${1:-normal}"
failures=0
pass() { printf 'PASS %s\n' "$1"; }
fail() { printf 'FAIL %s\n' "$1" >&2; failures=$((failures + 1)); }

if loginctl list-sessions --no-legend | awk '{print $1}' | while read -r session; do
    [[ "$(loginctl show-session "$session" -p Type --value 2>/dev/null)" == wayland ]] && exit 0
done; then
    pass 'a Wayland login session is active'
else
    fail 'no Wayland login session is active'
fi

[[ "$(pgrep -cx kwin_wayland || true)" == 1 ]] && pass 'one KWin Wayland process' || fail 'expected exactly one KWin Wayland process'
[[ "$(pgrep -cx plasmashell || true)" == 1 ]] && pass 'one AeroShell process' || fail 'expected exactly one plasmashell process'
[[ "$(qdbus6 org.kde.plasmashell /PlasmaShell org.kde.PlasmaShell.shell 2>/dev/null)" == io.gitgud.wackyideas.desktop ]] \
    && pass 'AeroShell is the visible shell package' || fail 'stock Plasma shell package is active'
systemctl --user is-active --quiet aero7-shell.service && pass 'Aero7 health service active' || fail 'Aero7 health service inactive'
systemctl --user is-active --quiet aero7-session-setup.service && pass 'Aero7 session layout applied' || fail 'Aero7 session setup inactive'

for process in aero7-desktop-surface aero7-taskbar aero7-start aero7-tray aero7-notify; do
    if pgrep -x "$process" >/dev/null 2>&1; then fail "duplicate $process process is active"; else pass "no duplicate $process process"; fi
done

layout_script='var out={screens:screenCount,panels:[],desktops:[]}; for (var p of panels()) {var w=[]; for(var x of p.widgets()) w.push(x.type); out.panels.push({type:p.type,screen:p.screen,widgets:w});} for (var d of desktops()) out.desktops.push(d.type); print(JSON.stringify(out));'
layout="$(qdbus6 org.kde.plasmashell /PlasmaShell org.kde.PlasmaShell.evaluateScript "$layout_script")"
if python - "$layout" <<'PY'
import json, sys
s=json.loads(sys.argv[1])
assert len(s['panels']) == max(1, s['screens']), s
expected=['io.gitgud.wackyideas.SevenStart','io.gitgud.wackyideas.seventasks','io.gitgud.wackyideas.systemtray','io.gitgud.wackyideas.digitalclocklite','io.gitgud.wackyideas.win7showdesktop']
for panel in s['panels']:
    assert panel['type'] == 'io.gitgud.wackyideas.panel', panel
    assert panel['widgets'] == expected, panel
assert len(s['desktops']) >= max(1, s['screens']), s
assert set(s['desktops']) == {'io.gitgud.wackyideas.desktopcontainment'}, s
PY
then pass 'exactly one ordered Aero taskbar per monitor'; else fail 'AeroShell layout is invalid'; fi

state_root="${XDG_STATE_HOME:-$HOME/.local/state}/aero7-desktop"
[[ -s "$state_root/health.json" ]] && pass 'health state written' || fail 'health state missing'
if [[ "$expectation" == --expect-safe ]]; then
    grep -Fq '"safe_mode": true' "$state_root/health.json" && pass 'safe mode recorded' || fail 'safe mode not recorded'
elif [[ "$expectation" != normal ]]; then
    printf 'Usage: %s [--expect-safe]\n' "$0" >&2
    exit 64
fi

if systemctl --user --failed --no-legend | grep -q .; then fail 'user manager contains failed units'; else pass 'no failed user units'; fi

if (( failures > 0 )); then
    printf '%s live session checks failed.\n' "$failures" >&2
    exit 1
fi
printf 'All AeroShell live-session checks passed.\n'
