#!/usr/bin/env bash
set -euo pipefail

pass() { printf 'PASS %s\n' "$1"; }
fail() { printf 'FAIL %s\n' "$1" >&2; exit 1; }
require_text() {
    local file=$1 text=$2 description=$3
    grep -Fq -- "$text" "$file" || fail "$description"
}
reject_text() {
    local file=$1 text=$2 description=$3
    ! grep -Fq -- "$text" "$file" || fail "$description"
}

volume=/usr/share/plasma/plasmoids/io.gitgud.wackyideas.volume/contents/ui/main.qml
network=/usr/share/plasma/plasmoids/io.gitgud.wackyideas.networkmanagement/contents/ui/main.qml
clock=/usr/share/plasma/plasmoids/io.gitgud.wackyideas.digitalclocklite/contents/ui/main.qml
calendar=/usr/share/plasma/plasmoids/io.gitgud.wackyideas.digitalclocklite/contents/ui/CalendarView.qml
wallpaper=/usr/share/wallpapers/Aero7/contents/images/1672x941.png

for file in "$volume" "$network" "$clock" "$calendar" "$wallpaper"; do
    [[ -f "$file" ]] || fail "required shell surface is missing: $file"
done

expected_wallpaper=83290605062f971385cdeca90aeba4dad361f5518c62c3b0b5ee63debf4d1599
actual_wallpaper=$(sha256sum "$wallpaper" | awk '{print $1}')
[[ "$actual_wallpaper" == "$expected_wallpaper" ]] \
    || fail 'desktop does not use the approved clean Aero7 target wallpaper'
pass 'clean Aero7 target wallpaper is installed without the center-logo overlay'

require_text "$volume" 'No audio output or input devices were found.' \
    'volume flyout lacks an understandable no-device state'
require_text "$volume" '/usr/bin/control --setting sound' \
    'volume settings do not route to the Aero7 Control Panel'
reject_text "$volume" 'KCMLauncher.openSystemSettings' \
    'volume surface still launches KDE System Settings directly'
pass 'volume flyout handles missing devices and routes settings through Control Panel'

require_text "$network" '/usr/bin/control --setting network-status' \
    'network footer does not route to Network and Sharing Center'
reject_text "$network" 'KCMLauncher.openSystemSettings' \
    'network surface still launches KDE System Settings directly'
pass 'network footer routes to Aero7 Network and Sharing Center'

require_text "$clock" '/usr/bin/control --setting ' \
    'clock setting helper does not launch the Aero7 Control Panel'
require_text "$clock" 'root.launchControlSetting("date-time")' \
    'clock does not route date/time settings to Control Panel'
require_text "$clock" 'root.launchControlSetting("region-language")' \
    'clock does not route regional formats to Control Panel'
require_text "$calendar" 'root.launchControlSetting("date-time")' \
    'calendar footer does not use the Aero7 date/time route'
reject_text "$clock" 'KCMLauncher.openSystemSettings' \
    'clock surface still launches KDE System Settings directly'
pass 'clock and calendar settings route through the Aero7 Control Panel'

printf 'All visible shell integration policy checks passed.\n'
