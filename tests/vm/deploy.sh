#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
remote_host="${1:-aero@localhost}"
remote_port="${2:-2222}"
remote_root="/tmp/aero7-desktop-deploy"
full_lifecycle="${AERO7_VM_FULL_PACKAGE_LIFECYCLE:-0}"
[[ "$full_lifecycle" == 0 || "$full_lifecycle" == 1 ]] || {
    printf 'AERO7_VM_FULL_PACKAGE_LIFECYCLE must be 0 or 1.\n' >&2
    exit 64
}
temporary_root="$(mktemp -d /tmp/aero7-desktop-deploy.XXXXXX)"
trap 'rm -rf -- "$temporary_root"' EXIT

ssh_options=(-o StrictHostKeyChecking=accept-new)
scp_options=(-o StrictHostKeyChecking=accept-new)
if [[ -n "${AERO7_SSH_KEY:-}" ]]; then
    ssh_options+=(-o IdentitiesOnly=yes -i "$AERO7_SSH_KEY")
    scp_options+=(-o IdentitiesOnly=yes -i "$AERO7_SSH_KEY")
fi

archive="$temporary_root/aero7-desktop.tar.gz"
tar --exclude=.git --exclude=build --exclude=pkg --exclude=src \
    -C "$project_root" -czf "$archive" .

scp -P "$remote_port" "${scp_options[@]}" \
    "$archive" "$remote_host:/tmp/aero7-desktop.tar.gz"
ssh -tt -p "$remote_port" "${ssh_options[@]}" "$remote_host" \
    "rm -rf -- '$remote_root' && mkdir -p '$remote_root' && tar -xzf /tmp/aero7-desktop.tar.gz -C '$remote_root' && AERO7_VM_FULL_PACKAGE_LIFECYCLE='$full_lifecycle' '$remote_root/tests/vm/install.sh'"

printf 'Deployment completed. Run graphical checks from docs/VM-TESTING.md.\n'
