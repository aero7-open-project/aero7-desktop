# Final validation record

Initial full-VM validation date: 2026-08-16. Publication-readiness refresh:
2026-08-24. The original table below records the package set used for that
full VM run; the newer parity packages are recorded separately so historical
evidence is not misrepresented as a new hardware or failure-path certification.

## Publication-readiness refresh

- Aero7 Desktop `0.2.0-13` configures, builds, passes all four CTest suites and
  produces a complete staged `/usr` installation from a clean build directory.
- The included Aero7 Desktop Gadgets 3.0 runtime builds independently, passes
  its nine-definition persistence/metadata self-test and produces the host,
  gallery alias, autostart entry and nine manifests in a staged installation.
- The required current parity stack is File Explorer `25.12.3-30`, Control
  Panel `0.1.0-23`, AeroShell `6.7.0_742.r9c2d850-35`, Gadgets `3.0.0-1`, and
  Internet Explorer compatibility `0.1.0-3`.
- Internet Explorer compatibility passed its source and desktop-entry tests,
  its full guest compatibility suite, Firefox/Chromium delegation checks, and
  a packaged 56-frame startup trace without a duplicate taskbar tile. Wayland
  activation remained present on the real taskbar launch.
- The r30/r23/r34 desktop stack passed a clean-login/reboot smoke test on
  2026-08-23. Gadgets 3.0 still needs a new live-session interaction pass; its
  source/build/self-test is verified here without claiming that missing test.

| Field | Result |
| --- | --- |
| Aero7 Desktop | `aero7-desktop 0.2.0-11` (`73b2116cf1eedef9ca37104169172f7c90684ddf46739c0bc4f3d3d3133a30c6`) |
| AeroShell theme | `aerothemeplasma-desktop-git 6.7.0_742.r9c2d850-20` (`8e30404e833a307f1bf3a3cba9ee9456171cd3c416686718d0d4b66461955b8b`) |
| Aero7 icons | `aerothemeplasma-icons-git 11.r96950b8-2` (`d4462055a972986eabf747d1d14773f39840379e4ce82950c71f44a1816df272`) |
| Control Panel | `linux-control-panel 0.1.0-13` (`2e51f117101172b89856b4c0000b1ca1248809298f843c87ee620e27351322d4`) |
| Gadgets | `aero7-gadgets 1.0.0-2` (`7c5322aa36bf4a861c7cc338a98a75c9a0f737c3fa7f18d23b1c749d6796e0c4`) |
| VM | Arch Linux x86-64, QEMU/KVM, 8 vCPU, 16 GiB, VirtIO GPU |
| Kernel | `7.1.8-arch1-3` |
| Plasma / KWin | `6.7.4-1` / `6.7.4-5` |
| KDE Frameworks / Qt / Wayland | `6.28.0-1` / `6.11.1-1` / `1.26.0-1` |
| File Explorer | `aero7-dolphin 25.12.3-2` |
| Full `pacman -Syu` audit | PASS, no repository updates pending |

## Corrected desktop architecture

The normal and safe Aero7 sessions use the real AeroShell package
`io.gitgud.wackyideas.desktop` on top of KWin/Plasma infrastructure. Each
active output has exactly one `io.gitgud.wackyideas.panel`, containing
SevenStart, SevenTasks, the Aero system tray, Aero clock, and Show Desktop in
that order. The rejected native overlay taskbar, Start, tray, notifications,
and desktop-surface processes are not started. Recovery restarts this same
AeroShell and removes any injected foreign Plasma panel.

The real Aero7 wallpaper, Windows 7 Aero icon theme, Aero7 pointer and sound
themes, Seven-Black Plasma theme, and SMod decoration are selected. The icon
theme no longer declares missing Oxygen themes as fallbacks. The Start menu
hides System Settings from browsing and search, uses real Aero7 icons, and
launches the separately maintained Control Panel and File Explorer.

## Validation results

- All five Control Panel source tests pass, including a synthetic two-output
  drag/rearrange test for the native Display page.
- The installed VM matrix passes session artifacts, live layout, shell route
  policy, Control Panel routes, KIO/File Explorer operations, personalization,
  window management, nested three-output layout, recovery, and failure
  injection. USB is the only skipped File Explorer case because no removable
  device is attached.
- Real KScreen tests pass output disable/re-enable and 1.25 scaling. The native
  Display page supports drag/rearrange, primary display, mode, refresh rate,
  orientation, scale, enable/disable, atomic Apply, and a 15-second
  confirmation rollback. Its VM interaction log records both dragged output
  positions in the applied and restore transactions.
- Start opens from the orb and Meta, toggles closed, searches applications,
  launches Control Panel/File Explorer, exposes All Programs and session
  commands, and remains a single instance.
- Volume, mute, mixer, network, hidden tray icons, clock/calendar, notification,
  grouped task buttons, thumbnail previews, Peek, pin persistence, Alt+Tab,
  Flip 3D, Snap, and recovery routes were exercised against the live Wayland
  session.
- Gadgets can be added from the Windows 7-style Desktop Gadgets chooser and
  persist across shell restart and reboot. Only the Aero7 Clock, CPU Meter,
  and Notes packages are exposed.
- After the final package installation and reboot, there are no failed user
  services or current Plasma/KWin crashes. The clean-boot audit has no warning
  that directly identifies Aero7, the Control Panel, or an
  `io.gitgud.wackyideas` component. Plasma and the hardware-free VM still emit
  the environment/framework diagnostics listed in `KNOWN-ISSUES.md`.
- The screenshot helper produced non-empty, fully opaque 1920x991 PNG files
  from the VM compositor.

Key visual evidence is retained in the local validation workspace under
`artifacts/vm-audit/`. Generated VM captures are intentionally ignored by Git
to keep the source repository small and to avoid treating screenshots as
runtime dependencies:

- `final-r20-clean-boot.png` and `final-r20-start-menu.png`;
- `final-context-menu.png`;
- `final-display-two-output.png`, `final-display-dragged.png`, and
  `final-display-apply-confirm.png`;
- `pkg10-network-control-panel.png`, `pkg10-volume-device.png`,
  `pkg10-volume-mixer.png`, and `pkg10-clock-control-panel.png`;
- `gadgets-added-hotpatch.png` and `gadgets-persist-hotpatch.png`;
- `pkg9-terminal-unlocked.png`.

## Remaining physical-hardware certification

The VM cannot certify a physical mixed-DPI display pair, real connector
hotplug, USB eject/reconnect, suspend/resume, physical audio-device switching,
Wi-Fi roaming, or pointer gesture feel on a hardware GPU. Those items remain
explicitly unclaimed; VM validation is PASS.
