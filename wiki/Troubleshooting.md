# Troubleshooting

## The Aero7 session does not appear in SDDM

Confirm that the desktop package is installed:

```bash
pacman -Q aero7-desktop
```

Finish any interrupted system update, then restart SDDM or reboot. The normal
and Safe Mode sessions are installed together.

## The package is not found

The package recipe is maintained on the Aero7 package repository's `beta`
branch, but the signed binary endpoint can lag during a beta publication
freeze. Check the repository status and refresh package databases with the
normal signed update process. Do not disable signature checking.

## The desktop layout is incomplete

1. Save open work.
2. Log out and try **Aero7 Desktop (Safe Mode)**.
3. Open `aero7-recovery-ui`.
4. Inspect status and logs before using restart or reset.

The recovery service repairs the Aero7 shell itself. A stock Plasma panel is
not the supported fallback.

## An application opens a KDE-looking advanced dialog

The normal settings route should open Aero7 Control Panel. A small number of
advanced settings can still use an individual KDE module as a compatibility
backend. Report a problem if a normal user-facing action opens the complete KDE
System Settings application or exposes desktop edit mode.

## Information to include in a bug report

Please include:

- the output of `pacman -Q aero7-desktop`;
- whether the normal session or Safe Mode was used;
- the number of displays, resolutions, and scale factors;
- exact steps to reproduce the problem;
- a screenshot when the issue is visual;
- whether it happens again after logging out and back in;
- relevant output from `aero7-recovery status` and
  `aero7-recovery logs --lines 200`.

Remove usernames, hostnames, file contents, and other private information
before attaching logs publicly.

Report problems in the
[Aero7 Desktop issue tracker](https://github.com/aero7-open-project/aero7-desktop/issues/new).
