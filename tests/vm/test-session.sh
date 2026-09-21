#!/usr/bin/env bash
set -euo pipefail

failures=0
expect_test_tools="${1:-}"
if [[ $# -gt 1 || ( -n "$expect_test_tools" && "$expect_test_tools" != --expect-test-tools ) ]]; then
    printf 'Usage: %s [--expect-test-tools]\n' "$0" >&2
    exit 64
fi
check_file() {
    if [[ ! -e "$1" ]]; then
        printf 'MISSING %s\n' "$1" >&2
        failures=$((failures + 1))
    else
        printf 'OK %s\n' "$1"
    fi
}
check_absent() {
    if [[ -e "$1" ]]; then
        printf 'STALE DUPLICATE %s\n' "$1" >&2
        failures=$((failures + 1))
    fi
}

check_file /usr/bin/aero7-session
check_file /usr/bin/aero7-migrate
check_file /usr/bin/aero7-recovery
check_file /usr/bin/aero7-recovery-ui
check_file /usr/bin/control
check_file /usr/bin/aero7-file-explorer
check_file /usr/share/applications/linux-controlpanel.desktop
check_file /usr/share/applications/org.aero7.FileExplorer.desktop
check_file /usr/share/locale/aero7/LC_MESSAGES/dolphin.mo
check_file /usr/lib/aero7-desktop/aero7-shell-service
check_file /usr/lib/aero7-desktop/aero7-session-setup
check_file /usr/share/aero7-desktop/shell/aero7-shell-layout.js
check_file /usr/share/wallpapers/Aero7/contents/images/1672x941.png
check_file /usr/share/wayland-sessions/aero7.desktop
check_file /usr/share/wayland-sessions/aero7-safe.desktop
check_file /usr/lib/systemd/user/aero7-shell.service
check_file /usr/lib/systemd/user/aero7-session-setup.service
check_file '/usr/lib/systemd/user/app-x\x2datpootb@autostart.service.d/10-aero7.conf'
if [[ "$expect_test_tools" == --expect-test-tools ]]; then
check_file /usr/lib/aero7-desktop/test-live-session.sh
check_file /usr/lib/aero7-desktop/test-recovery.sh
check_file /usr/lib/aero7-desktop/test-multimonitor.sh
check_file /usr/lib/aero7-desktop/test-control-panel.sh
check_file /usr/lib/aero7-desktop/test-file-explorer.sh
check_file /usr/lib/aero7-desktop/test-personalization.sh
check_file /usr/lib/aero7-desktop/test-shell-surfaces.sh
check_file /usr/lib/aero7-desktop/test-window-management.sh
check_file /usr/lib/aero7-desktop/test-failures.sh
check_file /usr/lib/aero7-desktop/test-upgrade.sh
check_file /usr/lib/aero7-desktop/test-reinstall.sh
check_file /usr/lib/aero7-desktop/test-uninstall.sh
check_file /usr/lib/aero7-desktop/aero7-test-status-notifier
check_file /usr/lib/aero7-desktop/aero7-screenshot-test
check_file /usr/share/applications/org.aero7.visualtest.desktop
check_file /usr/lib/udev/rules.d/99-aero7-ydotool.rules
fi
check_file /etc/xdg/aero7-desktop/aerothemeplasmarc
check_file /etc/xdg/aero7-desktop/kwinrc

for stale in \
    /usr/bin/aero7-control-panel /usr/bin/aero7-taskbar /usr/bin/aero7-start \
    /usr/bin/aero7-tray /usr/bin/aero7-notify /usr/bin/aero7-desktop-surface \
    /usr/share/applications/org.aero7.taskbar.desktop \
    /usr/share/applications/org.aero7.start.desktop \
    /usr/share/applications/org.aero7.tray.desktop \
    /usr/share/applications/org.aero7.notifications.desktop \
    /usr/share/applications/org.aero7.desktop.desktop \
    /usr/share/applications/org.aero7.controlpanel.desktop \
    /usr/share/applications/org.aero7.fileexplorer.desktop \
    /usr/share/dbus-1/services/org.aero7.Start.service \
    /usr/lib/systemd/user/aero7-taskbar.service \
    /usr/lib/systemd/user/aero7-start.service \
    /usr/lib/systemd/user/aero7-tray.service \
    /usr/lib/systemd/user/aero7-notify.service \
    /usr/lib/systemd/user/aero7-desktop-surface.service; do
    check_absent "$stale"
done

grep -Fqx 'Name=Aero7 Desktop' /usr/share/wayland-sessions/aero7.desktop || failures=$((failures + 1))
grep -Fqx 'X-KDE-SessionType=wayland' /usr/share/wayland-sessions/aero7.desktop || failures=$((failures + 1))
/usr/bin/aero7-session --invalid >/dev/null 2>&1 && failures=$((failures + 1))
python -m json.tool /usr/share/aero7-desktop/compatibility/desktop-stack.json >/dev/null

if (( failures > 0 )); then
    printf '%s VM session artifact checks failed.\n' "$failures" >&2
    exit 1
fi
printf 'AeroShell-first VM session artifact checks passed.\n'
