#!/usr/bin/env bash
set -euo pipefail
export LANG=C.UTF-8

unit=aero7-notify.service
interface=org.aero7.Notifications
object=/org/aero7/Notifications

fail() {
    printf 'FAIL %s\n' "$1" >&2
    systemctl --user status --no-pager "$unit" >&2 || true
    journalctl --user -u "$unit" -n 100 --no-pager >&2 || true
    exit 1
}
pass() { printf 'PASS %s\n' "$1"; }

state_json() {
    qdbus6 "$interface" "$object" org.freedesktop.Notifications.dumpState
}

wait_for_summary() {
    local expected=$1 state=''
    for _ in {1..40}; do
        state="$(state_json 2>/dev/null || true)"
        if python - "$state" "$expected" <<'PY'
import json, sys
try:
    state = json.loads(sys.argv[1])
except Exception:
    raise SystemExit(1)
raise SystemExit(0 if state.get("current", {}).get("summary") == sys.argv[2] else 1)
PY
        then
            printf '%s' "$state"
            return 0
        fi
        sleep 0.2
    done
    return 1
}

systemctl --user restart "$unit"
for _ in {1..40}; do
    state="$(state_json 2>/dev/null || true)"
    [[ "$state" == \{* ]] && break
    sleep 0.2
done
systemctl --user is-active --quiet "$unit" || fail 'Aero7 notification service did not start'
python - "$state" <<'PY' || fail 'notification server state is invalid'
import json, sys
state = json.loads(sys.argv[1])
assert state["schema"] == 1
assert isinstance(state["ownsFreedesktop"], bool)
assert isinstance(state["history"], list)
PY
pass 'notification service and compatibility mode are healthy'

qdbus6 "$interface" "$object" org.freedesktop.Notifications.clearHistory >/dev/null
notify-send -a 'Aero7 Integration Test' -t 0 -i dialog-information \
    'Aero7 standard notification' 'Markup <b>is sanitized</b>'
state="$(wait_for_summary 'Aero7 standard notification')" \
    || fail 'standard org.freedesktop.Notifications message did not reach Aero7'
python - "$state" <<'PY' || fail 'notification contents or sanitization are incorrect'
import json, sys
current = json.loads(sys.argv[1])["current"]
assert current["appName"] == "Aero7 Integration Test"
assert current["body"].strip() == "Markup is sanitized"
assert current["timeout"] == 0
PY
pass 'standard notifications reach Aero7 with safe plain-text content'

systemctl --user restart "$unit"
for _ in {1..40}; do
    state="$(state_json 2>/dev/null || true)"
    [[ "$state" == \{* ]] && break
    sleep 0.2
done
python - "$state" <<'PY' || fail 'notification history did not persist across restart'
import json, sys
history = json.loads(sys.argv[1])["history"]
assert any(row["summary"] == "Aero7 standard notification" for row in history)
assert len(history) <= 100
PY
pass 'notification history persists across process restart'

result_file="$(mktemp /tmp/aero7-notification-action.XXXXXX)"
sender_pid=''
cleanup() {
    if [[ -n "$sender_pid" ]] && kill -0 "$sender_pid" 2>/dev/null; then
        kill "$sender_pid" 2>/dev/null || true
    fi
    rm -f -- "$result_file"
}
trap cleanup EXIT
notify-send -a 'Aero7 Action Test' -t 0 -A accept=Accept -A reject=Reject \
    'Aero7 action notification' 'Select an action' >"$result_file" &
sender_pid=$!
state="$(wait_for_summary 'Aero7 action notification')" \
    || fail 'action-bearing standard notification did not reach Aero7'
read -r notification_id action_count < <(python - "$state" <<'PY'
import json, sys
current = json.loads(sys.argv[1])["current"]
print(current["id"], len(current["actions"]))
PY
)
[[ "$action_count" == 2 ]] || fail 'notification actions were not preserved'
qdbus6 "$interface" "$object" org.freedesktop.Notifications.invokeAction \
    "$notification_id" accept >/dev/null
for _ in {1..40}; do
    ! kill -0 "$sender_pid" 2>/dev/null && break
    sleep 0.2
done
kill -0 "$sender_pid" 2>/dev/null && fail 'action callback did not reach the sending application'
wait "$sender_pid"
sender_pid=''
[[ "$(<"$result_file")" == accept ]] || fail 'sender received the wrong notification action'
pass 'notification actions round-trip to the sending application'

qdbus6 "$interface" "$object" org.freedesktop.Notifications.toggleHistory '' >/dev/null
state="$(state_json)"
python - "$state" <<'PY' || fail 'notification history panel did not open'
import json, sys
assert json.loads(sys.argv[1])["historyVisible"] is True
PY
qdbus6 "$interface" "$object" org.freedesktop.Notifications.toggleHistory '' >/dev/null
qdbus6 "$interface" "$object" org.freedesktop.Notifications.clearHistory >/dev/null
pass 'history panel state and clear operation work'

printf 'All automated notification integration checks passed.\n'
