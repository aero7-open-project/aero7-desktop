#!/usr/bin/env bash
set -euo pipefail

output="${1:-$PWD/aero7-desktop-vm-logs}"
mkdir -p "$output"
cp -a "${XDG_STATE_HOME:-$HOME/.local/state}/aero7-desktop" "$output/user-state" 2>/dev/null || true
journalctl --user -u aero7-shell.service --no-pager >"$output/aero7-shell-journal.txt" || true
journalctl -b -u sddm --no-pager >"$output/sddm-journal.txt" || true
pacman -Q >"$output/packages.txt"
uname -a >"$output/uname.txt"
printf 'Logs collected in %s\n' "$output"
