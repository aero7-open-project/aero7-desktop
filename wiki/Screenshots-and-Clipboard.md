# Screenshots and Clipboard

## Capture a rectangle

1. Press **Meta+Shift+S** (the Windows-logo key is normally Meta).
2. Drag a rectangle around the area to capture.
3. Release the mouse to finish. The overlay closes; the Spectacle editor/main
   window does not open as part of this workflow.
4. Paste with **Ctrl+V** into an application that accepts images, or click the
   **Screenshot saved** notification to open the PNG in your default viewer.

The clipboard contains the actual image, including PNG data, not just a file
path. A plain-text editor cannot accept an image; use an image editor,
document editor, or another application with image-paste support. Copying
something else later replaces the active clipboard selection.

## Where screenshots are saved

Aero7 uses a **Screenshots** subfolder of your configured XDG **Pictures**
directory, normally `~/Pictures/Screenshots/`. Moving the Pictures location
changes this base path. It is not a hard-coded English home-directory path
and does not use an independently configured screenshots-directory setting.

The helper creates the folder if necessary and saves a PNG with a timestamp
and a short unique suffix. Each capture has its own filename. Open File
Explorer, go to Pictures, then open Screenshots to find earlier captures.

Press **Esc** in the selector to cancel. A cancelled selection should not
produce a saved-image notification. A missing or unwritable Pictures folder,
an unreadable output image, or a failed Spectacle launch is an error rather
than a successful capture.

## What provides this behavior

Spectacle supplies the Wayland rectangular selector. The Aero7 Snipping Tool
session helper starts it in background/release-capture mode, reads the saved
image into the clipboard, and issues the Plasma notification with an Open
action. A normal standalone Spectacle launch can still have its own UI and
preferences; it is not the same route as the Aero7 shortcut.

## Troubleshooting

| Symptom | Check |
| --- | --- |
| Shortcut does nothing | Confirm you are in the Aero7 session and that another application has not claimed Meta+Shift+S. Check Snipping Tool session startup. |
| Selector opens but nothing is saved | Try a deliberate drag and release, then check the Pictures/Screenshots folder permissions and available space. Esc cancels without saving. |
| File exists but Ctrl+V fails | Test a second image-capable application. Confirm another copy operation did not replace the clipboard and that the capture helper is still running. |
| No visible notification | Check notification settings and Do Not Disturb; inspect the destination folder before repeating a capture. |
| Notification cannot open the image | Set a working default PNG viewer under Control Panel's Default Programs/file associations. |
| Spectacle editor opens | Verify that the shortcut uses the Aero7 helper rather than an older direct Spectacle binding. Update the matching desktop/theme packages together. |

Screenshots may contain account names, filenames, network addresses, or
messages. Review the image before attaching it to a public issue.
