# Multi-monitor behavior

The AeroShell containment watches the live Wayland output list. Each output
gets one `io.gitgud.wackyideas.panel` and one Aero7 desktop containment.
Panels share launcher configuration, running-window state, tray policy,
wallpaper configuration, and gadget packages; output changes do not insert a
stock Plasma panel or lose the Aero7 layout.

Policy:

- every taskbar shows the same pinned and running applications;
- grouped windows, active/minimized/attention state, previews, and jump lists
  remain synchronized;
- Start opens on the output containing the pointer;
- the clock/tray appears consistently in each Aero7 taskbar view;
- desktop file icons appear on the primary output, while wallpaper appears on
  all outputs using the synchronized-background policy;
- gadgets persist an output connector name and position; gadgets created
  without an output are placed on the current primary desktop;
- Show Desktop and Peek are compositor-wide operations;
- output removal destroys only that output's views; state survives reconnect.

`test-multimonitor.sh` creates a nested three-output Wayland compositor,
verifies the exact ordered AeroShell panel on every output, disables and
re-enables one output, and checks 1.25 scaling. The native Display page
separately edits real KScreen positions, primary output, modes, refresh rate,
orientation, scaling, and enabled state with rollback. Its numbered monitor
diagram supports pointer drag/rearrange and is covered by both a synthetic
two-output interaction test and VM visual evidence.

Physical connector hotplug and GPU-driver combinations still require the
manual hardware checklist in `VM-TESTING.md`; a nested compositor cannot prove
every monitor/driver behavior.
