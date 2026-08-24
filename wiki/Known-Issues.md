# Known Issues

Aero7 Desktop is in beta. The following limits remain open and should not be
reported as completed hardware certification:

- VM screenshots and visual comparisons use virtual graphics; they do not
  replace physical-GPU inspection.
- Physical USB hardware, GPU combinations, suspend/resume, mixed-DPI displays,
  and real connector hotplug still need broader testing.
- Display testing covers extend, position, scale, primary, disable, and
  restore. Verified display mirroring is not currently claimed.
- Physical mouse gesture feel still needs hardware validation even though the
  window behavior is exercised by automated tests.
- Plasma can log an empty-menu timing diagnostic while SevenStart opens even
  though the menu becomes populated and remains functional.
- The signed pacman endpoint may temporarily lag behind the beta package
  recipes while a package set is frozen for testing.

These are explicit beta limits. They do not mean Aero7 activates a hidden
alternate desktop or silently substitutes non-working controls.
