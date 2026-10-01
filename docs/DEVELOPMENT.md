# Development

## Prerequisites and build

The maintained shell/theme source is in `theme/`. The top-level CMake build
includes it by default. `packaging/arch/dependencies.txt` lists the Arch
build/test dependencies used by CI. To build only the session integration,
set `-DAERO7_BUILD_THEME=OFF`; the separate theme package is built from
`theme/` in the same repository. Do not fetch the retired theme fork.

The tested development host is Arch Linux with CMake, Ninja, GCC, Qt 6,
Kirigami, KIO, KService, Solid, NetworkManagerQt, PulseAudioQt,
plasma-workspace/LibTaskManager, LayerShellQt, KPipeWire, gettext, Node.js,
ShellCheck, and desktop-file-utils.

```bash
tests/run.sh
```

`tests/ci/validate-static.sh` validates Bash, QML, KWin JavaScript, desktop and
session files, JSON, and Python syntax. CTest covers the pins parser, health
service, D-Bus/service behavior, migration, session artifacts, and the policy
which prohibits importing the retired script tree.

## Design rules

- Keep stable Linux/KDE backends and put Aero7 UI above public APIs.
- Add no button without a working backend and failure state.
- Keep long-running components in separate systemd failure domains.
- Write user state atomically under XDG config/state directories.
- Preserve configuration before migrations or dangerous display changes.
- Do not add generic Plasma customization to normal settings.
- Treat separately maintained Aero7 applications as runtime integrations, not
  source to vendor into this repository.

Use `apply_patch`-sized changes and run `git diff --check`, the static suite,
the build, and CTest before VM deployment. Graphical changes must also run the
relevant installed VM script. Public pull-request CI performs no privileged or
destructive host actions; package lifecycle and crash tests stay in the
dedicated disposable VM.

## Branch policy

Develop `aero7-desktop` and its package on an experimental/testing branch.
Promote only the version recorded in the compatibility manifest after the
complete VM matrix. Do not replace the stable Aero7 desktop solely because a
feature works in a local nested session.
