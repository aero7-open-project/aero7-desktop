#!/usr/bin/env bash
set -euo pipefail
export LANG=C.UTF-8

fail() { printf 'FAIL %s\n' "$1" >&2; exit 1; }
pass() { printf 'PASS %s\n' "$1"; }

wallpaper=/usr/share/wallpapers/Aero7/contents/images/1672x941.png
[[ -s "$wallpaper" ]] || fail 'real Aero7 wallpaper is missing'
[[ -f '/usr/share/icons/Windows 7 Aero/index.theme' ]] || fail 'Windows 7 Aero icon theme is missing'
find /usr/share/icons -maxdepth 2 -type f -path '*/aero-drop/index.theme' -print -quit | grep -q . \
    || fail 'aero-drop cursor theme is missing'
[[ -f /usr/share/sounds/Aero7/index.theme ]] || fail 'Aero7 sound theme is missing'
find /usr/share/sounds/Aero7 -type f \( -name '*.wav' -o -name '*.oga' -o -name '*.ogg' \) -print -quit | grep -q . \
    || fail 'Aero7 sound theme has no playable sounds'
pass 'real Aero7 wallpaper, icons, pointers, and sounds are installed'

[[ "$(kreadconfig6 --file kdeglobals --group Icons --key Theme)" == 'Windows 7 Aero' ]] || fail 'Aero icon theme is not selected'
[[ "$(kreadconfig6 --file kdeglobals --group Sounds --key Theme)" == Aero7 ]] || fail 'Aero sound theme is not selected'
[[ "$(kreadconfig6 --file kcminputrc --group Mouse --key cursorTheme)" == aero-drop ]] || fail 'Aero cursor theme is not selected'
[[ "$(kreadconfig6 --file plasmarc --group Theme --key name)" == Seven-Black ]] || fail 'Seven-Black Plasma theme is not selected'
[[ "$(kreadconfig6 --file kwinrc --group org.kde.kdecoration2 --key library)" == org.smod.smod ]] || fail 'SMod decoration library is not selected'
[[ "$(kreadconfig6 --file kwinrc --group org.kde.kdecoration2 --key theme)" == SMOD ]] || fail 'SMod decoration theme is not selected'
pass 'Aero visual defaults and SMod window chrome are selected'

runtime_wallpapers="$(qdbus6 org.kde.plasmashell /PlasmaShell org.kde.PlasmaShell.evaluateScript \
    'var out=[]; for(var d of desktops()){d.currentConfigGroup=["Wallpaper","org.kde.image","General"];out.push(d.readConfig("Image",""));} print(JSON.stringify(out));')"
python - "$runtime_wallpapers" "$wallpaper" <<'PY' \
    || fail 'AeroShell desktop containment does not reference the Aero7 wallpaper'
import json, sys
images=json.loads(sys.argv[1]); expected='file://' + sys.argv[2]
assert images and all(image == expected for image in images), images
PY
pass 'Aero7 wallpaper is applied to the clean desktop containment'

catalog="$(control --list-settings-json)"
python - "$catalog" <<'PY' || fail 'personalization routes are incomplete'
import json, sys
keys={row['key'] for row in json.loads(sys.argv[1])}
required={'personalization','colors','sound-theme','pointers','screen-lock','locations','wallpaper','icons'}
assert required <= keys, sorted(required-keys)
PY
pass 'real Control Panel owns the Personalization routes'
