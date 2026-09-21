#!/usr/bin/env bash
# SPDX-License-Identifier: LGPL-2.0-or-later
set -euo pipefail

if [[ $# -ne 1 || ! -d $1 ]]; then
    echo 'Usage: bash prepare.sh /absolute/path/to/pristine/kwallet-6.29.0' >&2
    exit 2
fi
integration_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)
source_dir=$(cd -- "$1" && pwd -P)
desktop_dir=$(cd -- "$integration_dir/../../.." && pwd -P)
icon_file="$desktop_dir/companions/aero7-credential-vault/icons/credential-vault.png"
daemon_dir="$source_dir/src/runtime/ksecretd"
[[ $(realpath -e -- "$daemon_dir") == "$daemon_dir" ]] || { echo 'Refusing symlinked source directories' >&2; exit 1; }

verify_file() {
    local expected=$1 file=$2 actual
    [[ -f $file && ! -L $file ]] || { echo "Missing or symlinked input: $file" >&2; exit 1; }
    actual=$(sha256sum -- "$file")
    [[ ${actual%% *} == "$expected" ]] || { echo "Unexpected input: $file" >&2; exit 1; }
}

# Refuse version drift and already-modified inputs before changing anything.
verify_file 8241cf9f0b60b038be9fef4018078bcb4e367f65f37895b0f5d5064b1c32f609 "$daemon_dir/ksecretd.cpp"
verify_file 10eb15d42dde57ce93aa166878b715f70b206895b3e5254c75fe12cd70b4c6f2 "$daemon_dir/knewwalletdialog.cpp"
verify_file b05509788d2ca1995ac553083def192eac7be3577788b06efcdb61a2a5654164 "$daemon_dir/CMakeLists.txt"
verify_file ee4f3b564acf5559cb73997e2ad9cd3a0c322b0f9b73ed3f0aaa4331c69b5d71 "$icon_file"
for target in aero7vaultpresentation.h credential-vault.png; do
    if [[ -e $daemon_dir/$target || -L $daemon_dir/$target ]]; then
        echo "Refusing to overwrite $daemon_dir/$target" >&2
        exit 1
    fi
done
patch --dry-run --batch --forward --fuzz=0 -d "$source_dir" -p1 < "$integration_dir/aero7-vault-presentation.patch"
patch --batch --forward --fuzz=0 -d "$source_dir" -p1 < "$integration_dir/aero7-vault-presentation.patch"
install -m 644 -- "$integration_dir/aero7vaultpresentation.h" "$daemon_dir/aero7vaultpresentation.h"
install -m 644 -- "$icon_file" "$daemon_dir/credential-vault.png"
