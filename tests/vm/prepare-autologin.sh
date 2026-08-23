#!/usr/bin/env bash
set -euo pipefail

login_user="${1:-aero}"
session_file="${2:-aero7.desktop}"

if ! getent passwd "$login_user" >/dev/null; then
    printf 'Unknown login user: %s\n' "$login_user" >&2
    exit 1
fi
if [[ ! -f "/usr/share/wayland-sessions/$session_file" ]]; then
    printf 'Unknown Wayland session: %s\n' "$session_file" >&2
    exit 1
fi

sudo install -d -m 0755 /etc/sddm.conf.d
printf '%s\n' \
    '[Autologin]' \
    "User=$login_user" \
    "Session=$session_file" \
    'Relogin=false' \
    | sudo tee /etc/sddm.conf.d/90-aero7-vm-autologin.conf >/dev/null
sudo systemctl enable NetworkManager.service sddm.service
printf 'Configured one-time-style VM autologin for %s using %s.\n' "$login_user" "$session_file"
