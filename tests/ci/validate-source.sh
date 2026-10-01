#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$project_root"

mapfile -d '' shell_files < <(find . -path './build' -prune -o -type f -name '*.sh' -print0)
for file in "${shell_files[@]}"; do
    bash -n "$file"
done
# Retired upstream installers are preserved as history, not an Aero7 install
# path. Check their syntax above; lint the scripts we maintain and execute.
maintained_shell_files=()
for file in "${shell_files[@]}"; do
    [[ "$file" == ./theme/deprecated/* ]] || maintained_shell_files+=("$file")
done
shellcheck --severity=error "${maintained_shell_files[@]}" shell/session/aero7-session services/session/aero7-session-setup

node --check kwin/scripts/aero7shake/contents/code/main.js
node --check kwin/scripts/aero7snap/contents/code/main.js
node --check services/session/aero7-wallpaper-defaults.js
node --check theme/plasma/shells/io.gitgud.wackyideas.desktop/contents/main.js

desktop-file-validate packaging/session/*.desktop tests/visual/*.desktop

python3 - <<'PY'
import ast
import json
from pathlib import Path

for path in Path('.').rglob('*.json'):
    if 'build' in path.parts or '.git' in path.parts:
        continue
    with path.open(encoding='utf-8') as handle:
        json.load(handle)
for directory in ('migrations', 'tests'):
    for path in Path(directory).rglob('*.py'):
        ast.parse(path.read_text(encoding='utf-8'), filename=str(path))
PY

if git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    git diff --check
fi
printf 'Source validation passed; no compilation or package build was performed.\n'
