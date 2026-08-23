# Aero7 Desktop Gadgets

Aero7 Desktop Gadgets is the native Windows 7-style gadget subsystem for the
Aero7 desktop environment. It is a shared Qt 6 host, not a set of Plasma
plasmoids and not an HTML/JavaScript gadget engine.

The runtime provides:

- a two-page Windows 7-style Desktop Gadget Gallery;
- a real Wayland desktop layer through LayerShellQt, with an X11 fallback;
- independent gadget instances, settings, opacity and always-on-top state;
- free dragging, edge/gadget snapping and multi-monitor migration;
- fading hover controls and the Windows 7 gadget context menu;
- XDG layout, feed and network-cache persistence;
- login restoration and D-Bus control through `org.aero7.Gadgets`;
- safe native backends for procfs, HTTPS feeds/APIs and MPRIS2.

Nine built-ins ship in the shared host: Calendar, Clock, CPU Meter, Currency,
Feed Headlines, Picture Puzzle, Slide Show, Weather and Media Center. A clean
profile starts with Clock and Weather at the upper-right of the primary screen.

Configuration is stored below `~/.config/aero7/gadgets/`; cached feeds,
exchange rates and weather are stored below `~/.cache/aero7/gadgets/`.

Build and test:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build
ctest --test-dir build --output-on-failure
```

The artwork is rendered from original Aero7 vector/painting code and does not
redistribute Microsoft gadget bitmap resources. Aero7 is independent from and
not endorsed by Microsoft Corporation.
