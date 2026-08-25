#!/usr/bin/env bash
set -euo pipefail

pass() { printf 'PASS %s\n' "$*"; }
fail() { printf 'FAIL %s\n' "$*" >&2; exit 1; }

command -v aero7-internet-explorer >/dev/null \
  || fail 'aero7-internet-explorer is not installed'
desktop=/usr/share/applications/aero7-internet-explorer.desktop
[[ -f "$desktop" ]] || fail 'permanent Internet Explorer desktop entry is missing'
grep -qx 'Name=Internet Explorer' "$desktop" \
  || fail 'launcher is not branded Internet Explorer'
grep -qx 'Exec=aero7-internet-explorer %U' "$desktop" \
  || fail 'launcher does not route URLs through the compatibility component'
grep -qx 'StartupNotify=false' "$desktop" \
  || fail 'launcher creates a duplicate startup task beside the permanent pin'
pass 'permanent Start/search/taskbar desktop identity is installed'

status="$(QT_QPA_PLATFORM=offscreen aero7-internet-explorer --status-json)" \
  || fail 'status API failed'
python - "$status" <<'PY' || exit 1
import json, sys
status=json.loads(sys.argv[1])
assert isinstance(status['backends'], list)
assert status['selectedDesktopId'] != 'aero7-internet-explorer.desktop'
for backend in status['backends']:
    assert backend['desktopId'].endswith('.desktop')
    assert backend['desktopId'] != 'aero7-internet-explorer.desktop'
PY
pass 'shared browser discovery API is valid and recursion-safe'

backend_count="$(python - "$status" <<'PY'
import json, sys
print(len(json.loads(sys.argv[1])['backends']))
PY
)"
(( backend_count > 0 )) || fail 'no modern browser backend is installed'
pass "$backend_count modern browser backend(s) detected"

capture="$(mktemp)"
trap 'rm -f -- "$capture"' EXIT
first_backend="$(python - "$status" <<'PY'
import json, sys
print(json.loads(sys.argv[1])['backends'][0]['desktopId'])
PY
)"
QT_QPA_PLATFORM=offscreen aero7-internet-explorer --set-backend "$first_backend" \
  || fail 'backend selection failed'
AERO7_IE_CAPTURE_LAUNCH="$capture" QT_QPA_PLATFORM=offscreen \
  aero7-internet-explorer --private 'https://example.com/a%20b?q=one%20two' \
  || fail 'safe private launch delegation failed'
python - "$capture" "$first_backend" <<'PY' || exit 1
import json, sys
with open(sys.argv[1], encoding='utf-8') as handle:
    launch=json.load(handle)
assert launch['desktopId'] == sys.argv[2]
assert launch['mode'] == 'private-window'
assert launch['urls'] == ['https://example.com/a%20b?q=one%20two']
PY
pass 'encoded URL and InPrivate action reached the selected backend without shell evaluation'

if command -v qdbus6 >/dev/null && pgrep -x plasmashell >/dev/null; then
  taskbar="$(qdbus6 org.kde.plasmashell /PlasmaShell \
    org.kde.PlasmaShell.evaluateScript \
    'for(var p of panels()){for(var w of p.widgets()){if(w.type==="io.gitgud.wackyideas.seventasks"){w.currentConfigGroup=["General"];print(JSON.stringify(w.readConfig("launchers",[])));}}}')"
  if grep -q 'aero7-internet-explorer.desktop' <<<"$taskbar"; then
    pass 'AeroShell taskbar retains the permanent Internet Explorer pin'
  else
    printf 'PENDING Internet Explorer is not pinned in this existing user profile\n'
  fi
else
  printf 'PENDING live AeroShell task model is unavailable\n'
fi

printf 'All available Internet Explorer compatibility checks passed.\n'
