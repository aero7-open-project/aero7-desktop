# Aero7 Welcome handoff candidate

These patches apply to the previously tested Aero7 Plasma Workspace 6.7.4-3.2
source candidate, which already contains `StartupSplashNotifier`. Apply
`panel-ready-splash-handoff.patch` and `ksplash-transparent-exit.patch` when
building the next Workspace candidate. The Aero7 look-and-feel source in the
The included `theme/plasma/look-and-feel/authui7/contents/splash/Splash.qml`
supplies the matching fade change in this repository's `testing` branch.

The shell now sends the final `desktop` stage only after layout loading and
every screen's desktop and panel containment report UI-ready. The splash
binary honors a theme-provided `splashExitDelayMs` (bounded to 2 seconds),
letting the Aero7 QML overlay fade transparent over the already drawn desktop.
Themes without this property keep their original immediate exit. The existing
30-second splash watchdog remains a recovery path for broken sessions.

This folder is source for the next package build, not a prebuilt package.
Do not install a loose `plasmashell` or `ksplashqml` binary on user systems:
rebuild and sign the complete matching Plasma Workspace package, then test a
clean VM login, rollback, and the final ISO package selection before release.
The local 6.7.4-3.2 VM proof is recorded in
[`docs/qa/LOGIN-HANDOFF-2026-10-01.md`](../../docs/qa/LOGIN-HANDOFF-2026-10-01.md).
