#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
errors: list[str] = []
for path in (ROOT / "src").rglob("*"):
    if path.suffix not in {".cpp", ".h", ".qml", ".ui"}:
        continue
    relative = path.relative_to(ROOT).as_posix()
    for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        allowed = relative == "src/SettingsDialog.cpp" and "backend.iconName" in line
        if "QIcon::fromTheme" in line and not allowed:
            errors.append(f"{relative}:{number}: unapproved theme icon")
desktop = ROOT / "data/aero7-internet-explorer.desktop"
for number, line in enumerate(desktop.read_text(encoding="utf-8").splitlines(), 1):
    if line.startswith("Icon=") and not line.startswith("Icon=aero7-"):
        errors.append(f"desktop:{number}: non-namespaced icon")
if errors:
    raise SystemExit("\n".join(errors))
