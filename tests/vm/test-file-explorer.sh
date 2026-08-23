#!/usr/bin/env bash
set -euo pipefail
export LANG=C.UTF-8

mkdir -p "$HOME/.cache"
work="$(mktemp -d "$HOME/.cache/aero7-file-explorer.XXXXXX")"
marker="aero7-file-explorer-test-$$"
first_unit="$marker-first"
second_unit="$marker-second"

cleanup() {
    pkill -x dolphin 2>/dev/null || true
    systemctl --user stop "$first_unit.service" "$second_unit.service" 2>/dev/null || true
    if [[ -f "$HOME/.ssh/authorized_keys" ]]; then
        sed -i "/$marker/d" "$HOME/.ssh/authorized_keys"
    fi
    if [[ -f "$work/ssh-config.backup" ]]; then
        cp -- "$work/ssh-config.backup" "$HOME/.ssh/config"
    else
        rm -f -- "$HOME/.ssh/config"
    fi
    if [[ -f "$work/known-hosts.backup" ]]; then
        cp -- "$work/known-hosts.backup" "$HOME/.ssh/known_hosts"
    else
        rm -f -- "$HOME/.ssh/known_hosts"
    fi
    chmod -R u+rwX -- "$work" 2>/dev/null || true
    rm -rf -- "$work"
}
trap cleanup EXIT
fail() { printf 'FAIL %s\n' "$1" >&2; exit 1; }
pass() { printf 'PASS %s\n' "$1"; }
run_desktop() { systemd-run --user --quiet --wait --pipe "$@"; }
kio() { run_desktop kioclient --noninteractive "$@"; }

command -v dolphin >/dev/null || fail 'the Aero7 File Explorer compatibility launcher is not installed'
command -v aero7-file-explorer >/dev/null || fail 'the Aero7 File Explorer launcher is not installed'
command -v kioclient >/dev/null || fail 'KIO command-line integration is not installed'
command -v ark >/dev/null || fail 'Ark archive integration is not installed'
[[ "$(sed -n '/^\[Desktop Entry\]/,/^\[/s/^Name=//p' /usr/share/applications/org.kde.dolphin.desktop | head -1)" == 'File Explorer' ]] \
    || fail 'the normal file manager launcher is not branded File Explorer'
version_line="$(run_desktop aero7-file-explorer --version | head -1)"
[[ -n "$version_line" ]] || fail 'Aero7 File Explorer could not start in the live Wayland session'
version="${version_line##* }"
pass "Aero7 File Explorer $version and its Windows-style launcher are installed"

mkdir -p "$work/source" "$work/destination" "$work/moved"
printf 'Aero7 KIO operation test\n' >"$work/source/original.txt"
kio copy "file://$work/source/original.txt" "file://$work/destination/"
cmp "$work/source/original.txt" "$work/destination/original.txt" \
    || fail 'KIO local copy changed file contents'
kio move "file://$work/destination/original.txt" "file://$work/moved/renamed.txt"
[[ ! -e "$work/destination/original.txt" && -f "$work/moved/renamed.txt" ]] \
    || fail 'KIO move/rename did not preserve the expected file'
pass 'local copy, move, and rename use real KIO file operations'

truncate -s 67108864 "$work/source/large.bin"
kio copy "file://$work/source/large.bin" "file://$work/destination/"
[[ "$(stat -c %s "$work/destination/large.bin")" == 67108864 ]] \
    || fail '64 MiB KIO copy was truncated'
pass 'large-file copy completed without truncation'

mkdir "$work/denied"
chmod 500 "$work/denied"
if kio copy "file://$work/source/original.txt" "file://$work/denied/" >"$work/permission.log" 2>&1; then
    fail 'KIO incorrectly reported success for an unwritable destination'
fi
chmod 700 "$work/denied"
[[ ! -e "$work/denied/original.txt" ]] || fail 'permission failure created an unexpected output file'
grep -Eiq 'denied|permission|write|create|access' "$work/permission.log" \
    || fail 'permission failure did not produce an understandable diagnostic'
pass 'permission failure is rejected with a readable error'

kio move "file://$work/moved/renamed.txt" trash:/
[[ ! -e "$work/moved/renamed.txt" ]] || fail 'trash action left the original file in place'
kio ls trash:/ | grep -F 'renamed.txt' >/dev/null || fail 'trashed file is not visible through the KIO Recycle Bin'
pass 'Recycle Bin operations use the real KIO trash backend'

archive="$work/aero7-test.zip"
extract="$work/extracted"
mkdir "$extract"
run_desktop ark --batch --changetofirstpath --add-to "$archive" "$work/source/original.txt"
[[ -s "$archive" ]] || fail 'Ark did not create an archive'
run_desktop ark --batch --destination "$extract" "$archive"
cmp "$work/source/original.txt" "$extract/original.txt" \
    || fail 'Ark archive extraction changed file contents'
pass 'archive create and extract actions use the installed Ark backend'

mkdir -p "$work/ssh" "$HOME/.ssh"
chmod 700 "$work/ssh" "$HOME/.ssh"
ssh-keygen -q -t ed25519 -N '' -C "$marker" -f "$work/ssh/id_ed25519"
touch "$HOME/.ssh/authorized_keys"
chmod 600 "$HOME/.ssh/authorized_keys"
cat "$work/ssh/id_ed25519.pub" >>"$HOME/.ssh/authorized_keys"
[[ ! -f "$HOME/.ssh/config" ]] || cp -- "$HOME/.ssh/config" "$work/ssh-config.backup"
[[ ! -f "$HOME/.ssh/known_hosts" ]] || cp -- "$HOME/.ssh/known_hosts" "$work/known-hosts.backup"
cat >>"$HOME/.ssh/config" <<EOF
Host localhost
    User $USER
    IdentityFile $work/ssh/id_ed25519
    IdentitiesOnly yes
EOF
chmod 600 "$HOME/.ssh/config"
ssh-keyscan -H localhost >>"$HOME/.ssh/known_hosts" 2>/dev/null
chmod 600 "$HOME/.ssh/known_hosts"
network_listing="$(systemd-run --user --quiet --wait --pipe \
    kioclient --noninteractive ls "sftp://$USER@localhost$work/source/")"
grep -F 'original.txt' <<<"$network_listing" >/dev/null \
    || fail 'KIO SFTP network location did not expose the remote file'
pass 'network location browsing works through the real KIO SFTP worker'

removable_path="${AERO7_TEST_REMOVABLE_PATH:-}"
if [[ -z "$removable_path" ]]; then
    removable_path="$(findmnt -rn -o TARGET | awk -v user="$USER" '$0 ~ "^/run/media/" user {print; exit}')"
fi
if [[ -n "$removable_path" && -d "$removable_path" ]]; then
    kio ls "file://$removable_path/" >/dev/null
    pass "mounted removable-media location is browsable ($removable_path)"
else
    printf 'SKIP no USB/removable volume is attached to this virtual machine\n'
fi

systemd-run --user --quiet --collect --unit="$first_unit" aero7-file-explorer --new-window "$work/source"
systemd-run --user --quiet --collect --unit="$second_unit" aero7-file-explorer --new-window "$work/destination"
windows_found=false
for _ in {1..50}; do
    process_count="$(pgrep -cx dolphin || true)"
    taskbar="$(qdbus6 org.kde.plasmashell /PlasmaShell org.kde.PlasmaShell.evaluateScript 'var result={};for(var panel of panels()){for(var widget of panel.widgets()){if(widget.type==="io.gitgud.wackyideas.seventasks"){widget.currentConfigGroup=["General"];result={groupingStrategy:widget.readConfig("groupingStrategy",0),groupPopups:widget.readConfig("groupPopups",false),showPreviews:widget.readConfig("showPreviews",false)};}}}print(JSON.stringify(result));')"
    if python - "$process_count" "$taskbar" 2>/dev/null <<'PY'
import json, sys
assert int(sys.argv[1]) >= 2, sys.argv[1]
state=json.loads(sys.argv[2])
assert str(state['groupingStrategy']) == '1', state
assert str(state['groupPopups']).lower() == 'true', state
assert str(state['showPreviews']).lower() == 'true', state
PY
    then
        windows_found=true
        break
    fi
    sleep 0.2
done
[[ "$windows_found" == true ]] || fail 'two File Explorer windows did not launch with Aero task grouping enabled'
pass 'multiple File Explorer windows launch with grouping, popup, and preview behavior enabled'

printf 'All automated Aero7 File Explorer integration checks passed.\n'
