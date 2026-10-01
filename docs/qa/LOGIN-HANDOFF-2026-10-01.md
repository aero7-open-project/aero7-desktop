# Welcome-to-desktop handoff — VM candidate check

Date: 1 October 2026. Target: Aero7 Beta 2 test VM, 1280×800, Wayland,
`plasma-workspace 6.7.4-3.2`. This is a development check, not an ISO or
package-repository release.

## Change tested

- Plasma shell no longer ends the splash at desktop-containment readiness;
  it waits for the saved panel to be visible and its UI to report ready.
- The Aero7 splash removes its black stage-5 cover. At the final stage it
  fades the whole transparent overlay out over 500 ms, revealing the desktop.
- The splash executable stays alive for the theme's bounded fade interval;
  other themes retain immediate exit.

The VM was booted from a copy-on-write overlay. Only the test guest received
the locally built `plasmashell`, `ksplashqml`, and Aero7 splash QML. Original
guest files were backed up under
`/var/tmp/aero7-welcome-rollback-20261001/`. The packaged branding image was
missing from that older guest theme, so the source's bundled `watermark.png`
was supplied for this test. No host binaries or published packages changed.

## Checks

- Workspace 6.7.4 source configured and `plasmashell`, `ksplashqml`, and
  `startupsplashnotifiertest` built successfully.
- `ctest -R startupsplashnotifiertest`: 1/1 passed.
- `qmllint` on Aero7 `Splash.qml`: passed.
- After a clean guest reboot and SDDM password login, frame sequence
  [welcome-transition-2026-10-01.png](welcome-transition-2026-10-01.png)
  shows the Welcome image still present immediately before the handoff, the
  panel and desktop underneath the translucent overlay, then the complete
  desktop. There is no black frame or taskbar-less desktop in that final
  splash-to-desktop interval.

## Remaining gate

The separate SDDM-to-session compositor switch showed a brief black frame
before the Welcome overlay appeared. The new code fixes the end of the
Welcome sequence; it does not yet eliminate that earlier compositor-switch
flash. A matching signed Workspace package, clean package upgrade/rollback,
and final ISO login test remain required before this is release-ready.
