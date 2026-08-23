#!/usr/bin/env bash
set -euo pipefail
package_file="${1:?package file is required}"

sudo pacman -R --noconfirm aero7-desktop
[[ ! -e /usr/share/wayland-sessions/aero7.desktop ]] \
    || { printf 'FAIL Aero7 session file remained after uninstall\n' >&2; exit 1; }
[[ -x /usr/bin/startplasma-wayland && -e /usr/share/wayland-sessions/plasma.desktop ]] \
    || { printf 'FAIL separately installed Plasma session was damaged by uninstall\n' >&2; exit 1; }
printf 'PASS uninstall removed Aero7 without removing the separate Plasma desktop\n'

sudo pacman -U --noconfirm "$package_file"
systemctl --user daemon-reload
systemctl --user reset-failed
systemctl --user set-environment PLASMA_DEFAULT_SHELL=io.gitgud.wackyideas.desktop
systemctl --user restart plasma-plasmashell.service
systemctl --user start aero7-session-setup.service aero7-shell.service
/usr/lib/aero7-desktop/test-session.sh
printf 'PASS reinstall after removal restored all Aero7 session artifacts\n'
