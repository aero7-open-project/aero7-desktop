# Arch VM testing

## Target

- Arch Linux x86-64 with SDDM and a Wayland-capable KWin stack;
- 4–8 vCPU, 8 GiB minimum, VirtIO/VirGL or software rendering;
- SSH user with sudo access;
- one normal display; the multi-output test creates three nested outputs.

After package installation and an Aero7 login, run:

```bash
/usr/lib/aero7-desktop/test-session.sh
/usr/lib/aero7-desktop/test-live-session.sh
/usr/lib/aero7-desktop/test-control-panel.sh
/usr/lib/aero7-desktop/test-file-explorer.sh
/usr/lib/aero7-desktop/test-personalization.sh
/usr/lib/aero7-desktop/test-shell-surfaces.sh
/usr/lib/aero7-desktop/test-window-management.sh
/usr/lib/aero7-desktop/test-recovery.sh
/usr/lib/aero7-desktop/test-failures.sh
/usr/lib/aero7-desktop/test-multimonitor.sh
```

For automated pointer interaction, the disposable VM uses `ydotool`. Install
`tests/vm/99-aero7-ydotool.rules` as
`/etc/udev/rules.d/99-aero7-ydotool.rules`, reload udev, and restart the
`ydotool` user service. This marks its combined virtual input device as both a
keyboard and mouse so KWin accepts button events; it is test infrastructure,
not an Aero7 runtime dependency.

`test-recovery.sh` deliberately kills the visible shell and proves that the
health service restores the same AeroShell package, one Aero panel per output,
and the Aero wallpaper. `test-failures.sh` injects a foreign stock panel and
proves it is removed without starting any retired overlay component.

`test-multimonitor.sh` starts a nested KWin compositor with three outputs,
loads the exact packaged AeroShell layout, disables and re-enables an output,
and repeats validation at 1.25 UI scaling. It does not change the host VM's
display configuration.

## Visual capture

Launch the packaged, KWin-authorized screenshot helper from its desktop file:

```bash
rm -f /tmp/aero7-visual-test.png
gio launch /usr/share/applications/org.aero7.visualtest.desktop
```

The file is written to `/tmp/aero7-visual-test.png`. Verify that it is a
non-empty 1920x991 (or current-output-size) image before treating it as visual
evidence. The validated VM successfully captured Start, Control Panel, File
Explorer, grouped tasks, Peek, and notifications.

## Physical follow-up

The VM cannot certify real USB media, physical mixed-DPI displays,
suspend/resume, or real audio/network hardware changes. Run those checks on
target hardware before a production hardware claim.
