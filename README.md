# Aero7 Desktop

Aero7 Desktop is the user-facing desktop environment for Aero7. It uses
modern Linux, Wayland, KWin, KDE Frameworks, and the reused AeroShell desktop
package underneath while providing a Windows 7-inspired Aero7 session.

The 0.2 development line provides dedicated normal and safe-mode sessions,
one AeroShell panel per monitor, SevenStart, SevenTasks, Aero tray/clock,
Windows 7-inspired KWin behavior, the real separately maintained Aero7
Control Panel, recovery, Aero7 File Explorer integration, and the native
Desktop Gadgets runtime.
KWin, KScreen, KIO, NetworkManager, PipeWire, UPower, Solid, and standard MIME
associations remain the real backends.

This repository was created independently. It does not import or execute the
retired desktop installer and contains no Microsoft binaries.

## Included Desktop Gadgets

The complete Aero7 Desktop Gadgets 3.0 source is included under
[`companions/aero7-gadgets`](companions/aero7-gadgets). It provides the
Windows 7-style gallery, login restoration, multi-monitor placement and nine
built-ins: Calendar, Clock, CPU Meter, Currency, Feed Headlines, Picture
Puzzle, Slide Show, Weather and Media Center. The Arch desktop package requires
the separately built `aero7-gadgets` package, so a normal Aero7 installation
does not silently omit the gadget subsystem.

The gadget artwork is drawn by original Aero7 code. No Microsoft gadget
resources are redistributed.

## Build and test

On the tested Arch stack:

```bash
tests/run.sh
```

This performs shell/static validation, QML validation, the C++ build, unit
tests, backend/service tests, migration tests, and session-artifact tests. A
staged install can be made with:

```bash
DESTDIR="$PWD/stage" cmake --install build
```

The included gadget runtime is validated separately:

```bash
cmake -S companions/aero7-gadgets -B gadgets-build -G Ninja -DBUILD_TESTING=ON
cmake --build gadgets-build
QT_QPA_PLATFORM=offscreen ctest --test-dir gadgets-build --output-on-failure
```

The full graphical and package lifecycle is run in the Arch validation VM:

```bash
AERO7_SSH_KEY=/path/to/key tests/vm/deploy.sh aero@localhost 2222
```

See [VM testing](docs/VM-TESTING.md), [recovery](docs/RECOVERY.md), and the
[validation record](docs/FINAL-VALIDATION.md) before promoting a stack.

## Status

The pinned Arch/Plasma VM stack is validated through installation, clean login,
safe mode, AeroShell, nested three-monitor, KWin, Control Panel, recovery,
upgrade, reinstall, and uninstall automation. KWin's authorized screenshot
interface also produced inspectable visual evidence for the desktop, Start,
Control Panel, File Explorer, grouped tasks, Peek, and notifications. Physical
USB/GPU/multi-monitor hardware remains outside the available VM, so 0.2 is a
validated development stack rather than a physical-hardware production
certification.

Aero7 is an independent open-source project. It is not affiliated with or
endorsed by Microsoft Corporation and does not contain Microsoft Windows.
