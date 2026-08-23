#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
previous_version="$(pacman -Q aero7-desktop 2>/dev/null | awk '{print $2}' | cut -d- -f1 || printf 'none')"
fingerprint="72C79ABBBBE96446DD3324042694BFE1090F4FD6"
key_source="$project_root/packaging/repository/aero7-repository.asc"

if [[ ! -r /etc/arch-release ]]; then
    printf 'This installer requires Arch Linux.\n' >&2
    exit 1
fi

sudo pacman -Syu --needed --noconfirm \
    base-devel cmake ninja python curl git gettext nodejs shellcheck desktop-file-utils

actual_fingerprint="$(gpg --show-keys --with-colons "$key_source" | awk -F: '$1 == "fpr" {print $10; exit}')"
if [[ "$actual_fingerprint" != "$fingerprint" ]]; then
    printf 'Repository signing key fingerprint mismatch.\n' >&2
    exit 1
fi
sudo pacman-key --add "$key_source"
sudo pacman-key --lsign-key "$fingerprint"

config_file="/etc/pacman.conf.d/aero7.conf"
sudo install -d -m 0755 /etc/pacman.conf.d
if [[ ! -f "$config_file" ]]; then
    printf '%s\n' \
        '[aero7]' \
        'SigLevel = Required DatabaseRequired' \
        'Server = https://memegeko.github.io/aero7-repo/$arch' \
        | sudo tee "$config_file" >/dev/null
fi
if ! grep -Fq "Include = $config_file" /etc/pacman.conf; then
    printf '\nInclude = %s\n' "$config_file" | sudo tee -a /etc/pacman.conf >/dev/null
fi
sudo pacman -Sy --noconfirm

"$project_root/tests/run.sh"

package_root="$(mktemp -d /tmp/aero7-desktop-package.XXXXXX)"
trap 'rm -rf -- "$package_root"' EXIT
version="$(sed -n 's/^project(aero7-desktop VERSION \([^ ]*\).*/\1/p' "$project_root/CMakeLists.txt")"
source_root="$package_root/aero7-desktop-$version"
mkdir -p "$source_root"
tar --exclude=.git --exclude=build --exclude=pkg --exclude=src \
    -C "$project_root" -cf - . | tar -C "$source_root" -xf -
tar -C "$package_root" -czf "$package_root/aero7-desktop-$version.tar.gz" \
    "aero7-desktop-$version"
cp "$project_root/packaging/arch/PKGBUILD" "$package_root/PKGBUILD"

(
    cd "$package_root"
    makepkg -s --noconfirm --cleanbuild
)
package_file="$(find "$package_root" -maxdepth 1 -type f \
    -name "aero7-desktop-$version-*.pkg.tar.*" ! -name '*-debug-*' -print -quit)"
test -n "$package_file"
sudo pacman -U --noconfirm "$package_file"

/usr/bin/aero7-migrate >/dev/null
"$project_root/tests/vm/test-upgrade.sh" "$previous_version" "$version"

if [[ "${AERO7_VM_FULL_PACKAGE_LIFECYCLE:-0}" == 1 ]]; then
    "$project_root/tests/vm/test-reinstall.sh" "$package_file"
    "$project_root/tests/vm/test-uninstall.sh" "$package_file"
fi

"$project_root/tests/vm/test-session.sh"
