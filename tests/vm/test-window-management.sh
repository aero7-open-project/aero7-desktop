#!/usr/bin/env bash
set -euo pipefail
export LANG=C.UTF-8
export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/run/user/$(id -u)}"
export WAYLAND_DISPLAY="${WAYLAND_DISPLAY:-wayland-0}"
export DBUS_SESSION_BUS_ADDRESS="${DBUS_SESSION_BUS_ADDRESS:-unix:path=$XDG_RUNTIME_DIR/bus}"
export XDG_CONFIG_DIRS="${XDG_CONFIG_DIRS:-$HOME/.config/kdedefaults:/etc/xdg/aero7-desktop:/etc/xdg/aerothemeplasma:/etc/xdg}"

fail() { printf 'FAIL %s\n' "$1" >&2; exit 1; }
pass() { printf 'PASS %s\n' "$1"; }
cleanup() {
    systemctl --user stop aero7-window-qterminal.service aero7-window-kcalc.service 2>/dev/null || true
    pkill -x qterminal 2>/dev/null || true
    pkill -x kcalc 2>/dev/null || true
}
trap cleanup EXIT

kwin_shortcut() {
    qdbus6 org.kde.kglobalaccel /component/kwin \
        org.kde.kglobalaccel.Component.invokeShortcut "$1" >/dev/null
    sleep 0.25
}

[[ "$(kreadconfig6 --file kwinrc --group org.kde.kdecoration2 --key library)" == org.smod.smod ]] \
    || fail 'SMOD window decoration is not selected'
[[ "$(kreadconfig6 --file kwinrc --group TabBox --key LayoutName)" == thumbnail_aero ]] \
    || fail 'Aero thumbnail Alt+Tab switcher is not selected'
[[ "$(kreadconfig6 --file kwinrc --group TabBoxAlternative --key LayoutName)" == flip3d ]] \
    || fail 'Flip 3D alternative switcher is not selected'
pass 'SMOD decoration, Aero Alt+Tab, and Flip 3D are configured'

for effect in aeroglassblur libkwin_effect_smodsnap; do
    [[ "$(qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.isEffectSupported "$effect")" == true ]] \
        || fail "$effect is unsupported"
    if [[ "$(qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.isEffectLoaded "$effect")" != true ]]; then
        [[ "$(qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect "$effect")" == true ]] \
            || fail "$effect could not load"
    fi
done
for script in smodpeekscript aero7shake aero7snap; do
    [[ "$(qdbus6 org.kde.KWin /Scripting org.kde.kwin.Scripting.isScriptLoaded "$script")" == true ]] \
        || fail "$script is not loaded"
done
pass 'supported Aero blur, snap, Peek, and Shake integrations are loaded'

systemd-run --user --quiet --collect --unit=aero7-window-qterminal qterminal
systemd-run --user --quiet --collect --unit=aero7-window-kcalc kcalc
for _ in {1..50}; do
    pgrep -x qterminal >/dev/null && pgrep -x kcalc >/dev/null && break
    sleep 0.1
done
pgrep -x qterminal >/dev/null && pgrep -x kcalc >/dev/null \
    || fail 'real Wayland test windows did not launch'

for shortcut in 'Window Maximize' 'Window Restore' 'Window Minimize' \
    'Window Quick Tile Left' 'Window Restore'; do
    kwin_shortcut "$shortcut"
done
pass 'maximize, restore, minimize, and Aero Snap shortcuts execute on real Wayland windows'

shortcuts="$(qdbus6 org.kde.kglobalaccel /component/kwin org.kde.kglobalaccel.Component.shortcutNames)"
grep -Fqx 'Aero7 Shake Active Window' <<<"$shortcuts" \
    || fail 'Aero Shake callback is not registered'
grep -Fqx 'Aero7 Snap Left Active Window' <<<"$shortcuts" \
    || fail 'Aero Snap fallback callback is not registered'
pass 'Aero Shake and Snap callbacks are registered with KWin'

kwin_shortcut 'Walk Through Windows'
kwin_shortcut 'Walk Through Windows Alternative'
pgrep -x plasmashell >/dev/null || fail 'window switching destabilized AeroShell'
pass 'primary Alt+Tab and alternative Flip 3D endpoints execute without destabilizing AeroShell'

printf 'All automated KWin window-management checks passed.\n'
