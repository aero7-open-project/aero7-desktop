# Desktop/theme consolidation QA — 1 October 2026

## Tested source and scope

The complete `testing` theme at `84c7aeff5444c73a27680370d123785acbf7c1c3`
was imported with its Git ancestry into `theme/`. All former GitHub branch heads
and upstream release tags were retained as namespaced archive tags. Accepted
package overlays were integrated into the source rather than reapplied during
packaging. Existing icons, artwork and licences were preserved.

This checks source/build consolidation and an upgrade of an existing disposable
Aero7 VM. It is not a new ISO, physical install, multi-monitor certification or
an exhaustive retest of every desktop feature. No release branch was merged.

## Build and installation checks

| Check | Result |
| --- | --- |
| Combined root CMake/Ninja build, theme enabled | Passed |
| Combined CTest suite, Qt offscreen/private D-Bus session | 43/43 passed |
| Standalone theme package CTest suite | 28/28 passed |
| Included gadgets build/tests | 8/8 passed |
| Browser identity backend build/tests | 4/4 passed |
| Maintained shell, QML, desktop-file and JSON checks | Passed |
| Deterministic source snapshot | Passed; Git/build output excluded |
| Staged install coverage | 16 required session/theme paths present |
| Package ownership | No overlapping installed regular files |
| Login/lock/splash branding | Identical accepted asset installed in all three |

The first staging run caught relative configuration paths placing defaults in
`/usr/etc`; these now install in `/etc`, with a staging regression assertion.
The first VM run caught a duplicate `queryDelay` id from the package-overlay
merge, which prevented Start from loading. It was removed without dropping the
120 ms search coalescing or recent-app proxy mapping. A C++ test compiles the
entire shipped SearchView: it failed on the duplicate and passes after repair.
Session-only install-profile tests explicitly disable the theme to avoid
rebuilding every theme component for each profile.

Logs: [combined CTest](consolidation-2026-10-01/combined-ctest.log),
[static checks](consolidation-2026-10-01/static.log).

## Disposable VM evidence

QEMU/KVM, UEFI, four vCPUs, 4 GiB memory and a 1920×1080 display. A new qcow2
overlay was used; the backing VM disk and the host's installed packages were
not modified. The packages were installed through normal `pacman -U`
dependency/conflict resolution and reinstalled after the Start repair.

Installed versions: `aero7-desktop 0.2.0-44` and
`aerothemeplasma-desktop-git 6.7.0_760-1`.

- Reboot, SDDM password login, taskbar, Start and lock/unlock worked.
- Start search found Media Player and Enter launched the correct native app.
- Media Player opened its library with bundled icons and Aero decoration.
- The screenshot shortcut launched Spectacle with region/background/release
  arguments. PNG saving and an image entry in Klipper completed, and no editor
  remained open. The remembered full-screen rectangle was used in this smoke
  test; arbitrary rectangle geometry and notification clicks are not newly
  certified by this consolidation pass.
- After the final reboot, system/user failed-unit lists were empty and the
  user journal contained no error-priority entries. The checked test interval
  had no core dumps.

Existing VM/Mesa, SVG and upstream service warning messages remain; this is
not a claim that the full journal is warning-free.

Screenshots: [SDDM](consolidation-2026-10-01/sddm-1920x1080.png),
[Start search](consolidation-2026-10-01/start-search-1920x1080.png),
[Media Player](consolidation-2026-10-01/media-player-1920x1080.png).

## Tested package hashes

```text
6d3015a4bbec104a255aa0e4b6ecc6f4a4aabdc0697f0cd4f5cfba67b3c95111  aero7-desktop-0.2.0-44-x86_64.pkg.tar.zst
cd187a23f18888786fe1308f35e0e8ddb89897053f1d3c7b55852f2d3e27e052  aerothemeplasma-desktop-git-6.7.0_760-1-x86_64.pkg.tar.zst
```

These are local QA artifacts, not a signed production binary release. Builder
recipes retain both existing package names and pin the tested combined source.
Retire the old public theme repository by archiving it only after verifying
the Desktop `testing` push and the preserved archive refs.

## Clean-container CI follow-up

The first GitHub packaging run used checkout's REST-archive fallback because
the minimal Arch container did not yet have Git. Installing Git afterwards
could not restore the absent repository metadata required by the deterministic
snapshot tool. Both jobs now install Git before checkout. The tool's strict
repository/source safety checks remain unchanged; no runtime/package payload
was modified by this CI-only correction.

The subsequent container checkout also needed an explicit `safe.directory`
entry: its files are mounted with the runner's ownership, while steps run as
container root, and checkout's exception is confined to its temporary Git
configuration. CI now trusts exactly `$GITHUB_WORKSPACE`, never a wildcard.
This keeps snapshot enumeration working without weakening normal local Git
ownership protections or modifying the developer's Git configuration.
