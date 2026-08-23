#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$project_root"

mapfile -d '' shell_files < <(find . -path './build' -prune -o -type f -name '*.sh' -print0)
for file in "${shell_files[@]}"; do
    bash -n "$file"
done
shellcheck --severity=error "${shell_files[@]}" \
    integration/file-explorer/aero7-file-explorer \
    shell/session/aero7-session

mapfile -t qml_files < <(find shell -type f -name '*.qml' -print | sort)
qmllint "${qml_files[@]}"

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
