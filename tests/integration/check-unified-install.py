#!/usr/bin/env python3
"""Verify that a combined staged install contains both halves of the desktop."""
from pathlib import Path
import sys

stage = Path(sys.argv[1]).resolve(strict=True)
required = (
    "usr/bin/aero7-session",
    "usr/bin/aero7-media-player",
    "usr/bin/aero7-snipping-tool",
    "usr/bin/aero7-sddm-accessibility",
    "usr/share/wayland-sessions/aero7.desktop",
    "usr/share/plasma/plasmoids/io.gitgud.wackyideas.SevenStart/contents/ui/main.qml",
    "usr/share/plasma/plasmoids/io.gitgud.wackyideas.seventasks/contents/ui/main.qml",
    "usr/share/plasma/shells/io.gitgud.wackyideas.desktop/contents/lockscreen/AuthUI.qml",
    "usr/share/plasma/look-and-feel/authui7/contents/splash/Splash.qml",
    "usr/share/plasma/look-and-feel/authui7/contents/images/aero7-package-branding.png",
    "usr/share/sddm/themes/sddm-theme-mod/Assets/aero7-branding-r3.png",
    "usr/share/sddm/themes/sddm-theme-mod/SMOD/Aero7OnScreenKeyboard.qml",
    "usr/share/Kvantum/Windows7Aero/Windows7Aero.svg",
    "usr/share/color-schemes/Aero.colors",
    "etc/xdg/kwalletrc",
    "etc/xdg/aero7-desktop/aerothemeplasmarc",
)
missing = [name for name in required if not (stage / name).is_file()]
if missing:
    raise SystemExit("Missing combined install files: " + ", ".join(missing))

authui = stage / "usr/share/plasma/look-and-feel/authui7"
branding = (authui / "contents/images/aero7-package-branding.png").read_bytes()
for name in (
        "usr/share/sddm/themes/sddm-theme-mod/Assets/aero7-branding-r3.png",
        "usr/share/plasma/shells/io.gitgud.wackyideas.desktop/contents/images/branding.png"):
    if (stage / name).read_bytes() != branding:
        raise SystemExit(f"Login/lock branding differs: {name}")
if (authui / "contents/images/watermark.png").exists():
    raise SystemExit("Retired splash branding was installed")
if (stage / "usr/etc").exists():
    raise SystemExit("System configuration was installed under /usr/etc")
if list(stage.rglob(".git")):
    raise SystemExit("Git metadata leaked into the staged installation")
print(f"Combined session/theme staged install passed: {len(required)} required files")
