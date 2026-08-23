# Packaging

The Arch recipe is `packaging/arch/PKGBUILD`. It builds with CMake/Ninja, runs
CTest in `check()`, installs session, desktop, and systemd artifacts, and links
the setup and AeroShell health services into `graphical-session.target.wants`.

Runtime dependencies deliberately include the tested infrastructure and
separate Aero7 components: KWin/Plasma Workspace, Qt/KF6, LayerShellQt,
KPipeWire, NetworkManager/PipeWire/UPower/Solid, AeroTheme compatibility,
Control Panel backends, Device Manager, Computer Management,
`aero7-file-explorer`, `aero7-gadgets`, and Ark. Programs Center remains
optional; the native gadget runtime is part of the required desktop stack.

The gadget package is built from `companions/aero7-gadgets`. It intentionally
remains a separate package so it can be updated or restarted without replacing
the session package, while its complete source and CI test are maintained in
this repository.

`tests/vm/deploy.sh` creates a source archive from this repository, builds it
inside the Arch VM, runs package tests, imports the pinned Aero7 repository
signing key, installs the signed dependency set, and validates installed
artifacts. The package lifecycle scripts cover N→N+1 migration, reinstall,
removal, separate Plasma session availability, and final reinstall.

The public CI package job runs `makepkg` as an unprivileged build user with
`--nodeps` only because the private/signed Aero7 repository is unavailable to
GitHub's generic Arch container. The authoritative dependency and signature
test is the clean VM deployment.

Never replace the repository key without updating its fingerprint and
provenance in `THIRD_PARTY.md`. Never add Microsoft resources to the package.
