#!/usr/bin/env bash
set -euo pipefail

fail() { printf 'FAIL %s\n' "$1" >&2; exit 1; }
pass() { printf 'PASS %s\n' "$1"; }

systemctl --user is-active --quiet aero7-shell.service || fail 'AeroShell health service is not active'
old_pid="$(pgrep -xo plasmashell)"
[[ "$old_pid" =~ ^[1-9][0-9]*$ ]] || fail 'plasmashell has no PID'
kill -KILL "$old_pid"

new_pid=''
for _ in {1..100}; do
    new_pid="$(pgrep -xo plasmashell || true)"
    if [[ "$new_pid" =~ ^[1-9][0-9]*$ && "$new_pid" != "$old_pid" ]]; then break; fi
    sleep 0.25
done
[[ "$new_pid" =~ ^[1-9][0-9]*$ && "$new_pid" != "$old_pid" ]] || fail 'health service did not restart plasmashell'
[[ "$(qdbus6 org.kde.plasmashell /PlasmaShell org.kde.PlasmaShell.shell)" == io.gitgud.wackyideas.desktop ]] \
    || fail 'recovery started a stock Plasma shell'
pass "health service restarted the same AeroShell package ($old_pid -> $new_pid)"

for _ in {1..80}; do
    layout="$(qdbus6 org.kde.plasmashell /PlasmaShell org.kde.PlasmaShell.evaluateScript 'print(JSON.stringify({screens:screenCount,panels:panels().length,panelType:panels().length?panels()[0].type:"",desktops:desktops().length}));' 2>/dev/null || true)"
    if python - "$layout" <<'PY' 2>/dev/null
import json, sys
s=json.loads(sys.argv[1]); assert s['panels']==max(1,s['screens']); assert s['panelType']=='io.gitgud.wackyideas.panel'; assert s['desktops']>=max(1,s['screens'])
PY
    then break; fi
    sleep 0.25
done
python - "$layout" <<'PY' || fail 'AeroShell layout was not restored'
import json, sys
s=json.loads(sys.argv[1]); assert s['panels']==max(1,s['screens']); assert s['panelType']=='io.gitgud.wackyideas.panel'; assert s['desktops']>=max(1,s['screens'])
PY
pass 'recovery restored one Aero panel per output'

wallpapers="$(qdbus6 org.kde.plasmashell /PlasmaShell org.kde.PlasmaShell.evaluateScript 'var o=[];for(var d of desktops()){d.currentConfigGroup=["Wallpaper","org.kde.image","General"];o.push(d.readConfig("Image",""));}print(JSON.stringify(o));')"
python - "$wallpapers" <<'PY' || fail 'recovery did not restore the Aero wallpaper'
import json, sys
images=json.loads(sys.argv[1]); assert images and all('/wallpapers/Aero7/' in image for image in images), images
PY
pass 'recovery reasserted the Aero7 wallpaper'

aero7-recovery restart-shell
systemctl --user is-active --quiet aero7-shell.service || fail 'recovery CLI did not restore supervision'
pass 'recovery CLI restarts the complete AeroShell session without a stock fallback'
