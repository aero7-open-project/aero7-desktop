# Security

- Backend values must be validated desktop IDs and must resolve to a detected
  browser desktop entry.
- `aero7-internet-explorer.desktop` is always rejected as its own backend to
  prevent default-browser recursion.
- URLs and local HTML paths are represented as `QUrl` objects and passed to
  KIO as an argument list. They are never evaluated or concatenated into a
  shell command.
- Browser launches use desktop application definitions, preserving Flatpak and
  desktop-environment launch behavior.
- MIME defaults are changed only through explicit user actions. Existing
  defaults are recorded before replacement and can be restored.
- Launch logging records backend ID, mode, URL count and URL schemes. It does
  not record complete URLs, query strings, fragments or browser history.
- Browser packages, executables, icons and package-owned desktop files are not
  modified.
- Per-user settings do not require administrator privileges. Optional policy is
  separate and read-only to normal users.
