#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$project_root"

mapfile -d '' shell_files < <(find . -path './build' -prune -o -type f -name '*.sh' -print0)
for file in "${shell_files[@]}"; do
    bash -n "$file"
done
shellcheck --severity=error "${shell_files[@]}" shell/session/aero7-session

mapfile -t qml_files < <(find shell -type f -name '*.qml' -print | sort)
qmllint_binary="$(command -v qmllint || true)"
if [[ -z "$qmllint_binary" && -x /usr/lib/qt6/bin/qmllint ]]; then
    qmllint_binary=/usr/lib/qt6/bin/qmllint
fi
[[ -n "$qmllint_binary" ]] || {
    printf 'qmllint was not found in PATH or /usr/lib/qt6/bin.\n' >&2
    exit 1
}
"$qmllint_binary" "${qml_files[@]}"

node --check kwin/scripts/aero7shake/contents/code/main.js
node --check kwin/scripts/aero7snap/contents/code/main.js

desktop-file-validate packaging/applications/*.desktop packaging/session/*.desktop

python - <<'PY'
import json
from pathlib import Path

for path in Path('.').rglob('*.json'):
    if 'build' in path.parts or '.git' in path.parts:
        continue
    with path.open(encoding='utf-8') as handle:
        json.load(handle)
PY

python -m compileall -q migrations tests
if git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    git diff --check
fi
printf 'Static, QML, script, desktop-file, and configuration validation passed.\n'
