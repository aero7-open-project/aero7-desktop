#!/usr/bin/env bash
set -euo pipefail

install_root="${1:?install root is required}"
source_root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

install -Dm0644 \
    "$source_root/systemd/10-aero7-atpootb.conf" \
    "$install_root/lib/systemd/user/app-x\x2datpootb@autostart.service.d/10-aero7.conf"
