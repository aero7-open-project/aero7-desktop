#!/usr/bin/env bash
set -euo pipefail
project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$project_root"
bash tests/ci/validate-source.sh
# Qt-dependent checks stay local or on the dedicated builder, not GitHub.
mapfile -t qml_files < <(find shell -type f -name '*.qml' -print | sort)
qmllint_binary="$(command -v qmllint || true)"
if [[ -z "$qmllint_binary" && -x /usr/lib/qt6/bin/qmllint ]]; then
    qmllint_binary=/usr/lib/qt6/bin/qmllint
fi
[[ -n "$qmllint_binary" ]] || { printf 'qmllint was not found.\n' >&2; exit 1; }
"$qmllint_binary" "${qml_files[@]}"
printf 'Local/builder QML validation passed.\n'
