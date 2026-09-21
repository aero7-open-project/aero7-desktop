# First Login and Desktop Defaults

This page describes the Beta 2 source defaults. An upgraded account keeps its
personal settings where possible; it is not forcibly made to look like a new
account on every login. See [Screenshots](Screenshots) for the exact versions
and preparation used in the documentation VM.

## The first desktop

The intended factory layout is a bottom, full-width, 40-pixel taskbar with
the Start orb followed by **Command Prompt**, **File Explorer**, and
**Internet Explorer** pins, in that order. Running applications appear beside
or grouped with their pins. The right side contains the notification area,
clock/date, and the narrow **Show desktop** button.

The desktop starts with **Recycle Bin** in the upper-left corner. An Internet
Explorer launcher is available in Start and on the taskbar; this does not mean
a browser shortcut must be placed on the fresh desktop. Gadgets are available
from the gallery and can be added deliberately. Diagnostic test images are an
exception: they can also place an **Aero7 Physical Install Logs** folder on the desktop.

The shell does not expose Plasma desktop edit mode. Taskbar **Lock the
taskbar** is a familiar taskbar preference, not a way to enter Plasma edit
mode. A permanent brightness icon is excluded from the notification area;
supported brightness controls remain in display/power settings and hardware
keys.

## Your own layout

Use application jump lists to pin or unpin supported applications. Desktop
files, personal shortcuts, and saved gadget positions are your own data.
Migration recognizes certain previous factory pin lists, but should preserve
custom pin lists instead of resetting them at each login.

Do not delete all of `~/.config` to repair a pin. Check the package and desktop
entry first, then use [Safe Mode and Recovery](Safe-Mode-and-Recovery) if the
shell itself needs repair. File Explorer's current desktop entry is
`org.aero7.FileExplorer.desktop`; capitalization matters.

## Display and appearance

Use **right-click desktop > Screen resolution** to select the monitor and its
advertised mode. Scaling is separate from resolution. A 1920×1080 monitor at
100% scale has more usable workspace than the same mode at 150% scale.
See [Displays and Multi-Monitor](Displays-and-Multi-Monitor) before changing
several outputs at once.

**Personalize** opens the Aero7 appearance routes. Aero7 application identity
icons come from the project's existing approved AeroThemePlasma icon pack and
are bundled with the maintained applications. Changing the global icon theme
should not replace those application identities. This is not a promise that
every third-party application's internal icons use Aero7 artwork.

## Login, locking, and fallback sessions

At SDDM, use the button in the lower-left corner for the session menu and
**On-Screen Keyboard**. Select a session before signing in:

| Session | When to choose it |
| --- | --- |
| Aero7 Desktop | Normal Aero7 Wayland desktop. |
| Aero7 Desktop (Safe Mode) | The same shell with optional effects/extensions restricted for recovery. |
| AeroThemePlasma (Wayland) | Explicit themed Plasma fallback, when installed. |
| Plasma (Wayland) | Explicit stock Plasma fallback, when installed. |

The selected session must actually be installed to appear. Safe Mode is not
stock Plasma, and the health service does not silently switch you to a
different desktop. Save your work and sign out before changing environments.

The lock screen resumes the existing session after authentication; it is not
a session switcher. Login and lock-screen branding are separate installed
surfaces, so update both through the matching Aero7 packages if one still
shows an older logo.

The current SDDM menu supplies the on-screen keyboard and session choices.
It is **not** the full Windows 7 Ease of Access dialog: do not expect a working
pre-login Narrator, Magnifier, High Contrast, Sticky Keys, and Filter Keys
checkbox sheet. Desktop accessibility settings and optional screen-reading
services are separate, documented in [Optional Features](Optional-Features).
