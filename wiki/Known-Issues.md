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

## Superseded September documentation-VM observations

The [1080p gallery](Screenshots) uses local test packages and a retained
account, not the final Beta 2 release media. That older capture set shows duplicate browser
entries/QTerminal wording in Start, clipped text in older gadget thumbnails,
and a blank SDDM display-name area for the saved account. The login and lock
logos are fully visible at the captured scale. These observations need their
own follow-up; screenshots do not prove the corresponding migration or
fresh-install paths are fixed. The later 21 September clean online/offline
acceptance verifies the selected Desktop 33 taskbar/Explorer identity and
correct SDDM/lock branding, but it does not rewrite the historical screenshots.

The main [capture record](https://github.com/aero7-open-project/aero7/wiki/Screenshot-Capture-Details)
also records Control Panel icon mappings, Explorer's clipped hardware summary,
and the optional-package cache missing from that VM.
