# Desktop Gadgets

Open **right-click desktop > Gadgets**, or **Control Panel > Desktop
Gadgets**, to open the gallery. Aero7's current gadget runtime is a shared
native Qt host, not a browser engine or a collection of old Plasma widgets.
Windows `.gadget` packages are not supported extensions.

## Built-in gadgets

| Gadget | Purpose and important limits |
| --- | --- |
| Calendar | Shows the date and lets you browse months. It is not a calendar-account or appointment synchronization client. |
| Clock | Analog time display with clock-face and label settings. Uses the system's clock; change system time in Control Panel. |
| CPU Meter | Shows current CPU/memory use from Linux system data. In a VM these figures describe the guest, not the whole host computer. |
| Currency | Displays exchange-rate information from its network backend. Cached information is not a live trading quote. |
| Feed Headlines | Reads configured supported HTTPS feeds and opens links in the default browser. Network failures must not be mistaken for an empty news day. |
| Picture Puzzle | A local picture-tile puzzle. Its controls and image settings are separate from your file manager. |
| Slide Show | Displays pictures from a selected local folder. Choose an existing folder containing supported images; an empty folder cannot produce a slideshow. |
| Weather | Shows weather for the configured location using a network service. Confirm the location and units; it is not a local physical sensor. |
| Media Center | Controls compatible running media players through MPRIS2. It is not Microsoft's Windows Media Center, a television tuner, or a media streaming subscription. |

## Arrange and configure

Add a gadget from the gallery, then drag it to a useful desktop position.
Instances have their own settings. Hover controls and the gadget context menu
provide available options, opacity, always-on-top behavior, and close/remove
actions. Closing a gadget does not uninstall the Gadgets package.

The host remembers instances, settings, positions, and the chosen display.
It supports edge/gadget snapping and repositioning when a display disappears.
Avoid placing gadgets over Recycle Bin or the taskbar. A configured demo with
several gadgets is not the intended clean factory desktop.

## Storage and network use

Layout and settings are stored below `~/.config/aero7/gadgets/`. Feed,
exchange-rate, and weather caches are stored below `~/.cache/aero7/gadgets/`.
Keep a copy of the configuration before resetting it.

Clock, Calendar, CPU Meter, and a local puzzle/slideshow do not require live
internet data. Weather, Currency, and Feed Headlines need access to their
configured services for fresh information. Cached information can be stale;
read the status and refresh indicators. Media controls need a compatible
player, and the player's content may separately require internet access.

## Upgrading from older test builds

Old development VMs may still contain `org.aero7.gadgets.*` Plasma widget
instances. Those are not the current native host. If an upgrade leaves an
"Error loading applet" placeholder, record the old/new package versions and
remove the obsolete widget instance from that test profile; do not install
unrelated widget packs to conceal the error. The screenshot VM was prepared
by removing those legacy instances and using the native gallery.

See [Known Issues](Known-Issues) for the distinction between a successful VM
capture and physical multi-monitor validation.
