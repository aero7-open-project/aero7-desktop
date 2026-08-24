# Displays and Multi-Monitor

Aero7 watches the live Wayland output list and creates one Aero desktop and one
Aero taskbar for each active display.

## Synchronized behavior

- Every taskbar shares pinned and running applications.
- Grouped windows, active/minimized/attention state, previews, and jump lists
  stay synchronized.
- Start opens on the display containing the pointer.
- Clock and notification-area policy remain consistent across taskbars.
- Wallpaper appears on all displays; desktop file icons stay on the primary
  display.
- Gadgets remember the connector name and position used when they were placed.
- Show Desktop and Peek operate across the compositor.
- Disconnecting a display removes only that display's surfaces and preserves
  the layout state for reconnect.

## Display settings

Open **Screen Resolution** through Control Panel or the desktop context menu.
The Aero7 display page uses the real KScreen backend for resolution, refresh
rate, orientation, scaling, primary-display selection, enable/disable, and
display arrangement. Risky changes use a timed confirmation so the previous
layout can be restored.

## Current testing boundary

Nested Wayland testing covers three outputs, disabling and re-enabling an
output, layout changes, primary-display changes, and 1.25 scaling. Physical
connector hotplug, mixed-DPI combinations, and individual GPU drivers still
need hardware testing. Safe output cloning is not claimed because the public
backend does not expose a verified cloning transaction.
