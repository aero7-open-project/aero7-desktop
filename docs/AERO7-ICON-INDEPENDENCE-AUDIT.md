# Aero7 Icon Independence Audit

Status: implementation, automated tests, and required VM application-theme
matrix complete; live notification-toast acceptance follow-up remains,
2026-09-03

## Policy boundary

Aero7-owned application identity, window chrome, navigation, categories,
commands, statuses, dialogs, tray entries, and notifications must use icons
bundled with Aero7 code. Changing the Plasma icon theme must not change them.

Theme-aware lookup remains valid only for content not owned by Aero7: user
files and folders, MIME types, third-party applications, browser backends,
removable media, and icons supplied by external notification senders.

## Authoritative repositories inspected

| Application surface | Repository or source tree | Pre-change dependency |
|---|---|---|
| Programs Center | `aero7-open-project/aero7-programs-center` | Toolbar, sidebar, category, status, and action icons use `QIcon::fromTheme`; package artwork is also theme-aware. |
| Control Panel, Getting Started, Personalization, Folder Options, Taskbar and Start Menu, Action Center, Optional Features, and update pages | `aero7-open-project/aero7-control-panel-`, current feature worktree | `IconHelper.h` resolves every internal icon through the selected theme; four launchers use generic icon names; several window, tray, status, and dialog icons use the helper. |
| Device Manager | `aero7-open-project/aero7-device-manager` | `IconHelper.h` resolves the complete device tree and toolbar through the selected theme; launcher and window use generic names. |
| Computer Management | `aero7-open-project/aero7-computer-management` | Window, toolbar, navigation tree, management tasks, drive tiles, and eleven launchers use generic theme names. |
| Gadget Gallery and Gadget Host | `aero7-open-project/aero7-desktop`, `companions/aero7-gadgets` | Gallery window, online action, and all launchers use generic theme names. |
| Internet Explorer compatibility launcher | `aero7-open-project/aero7-internet-explorer` and the synchronized desktop companion | App artwork is installed in hicolor but window/settings code re-resolves it through the active theme; jump-list action icons are generic. Browser backend artwork is external and intentionally theme-aware. |
| Snipping Tool | `memegeko/aerothemeplasma`, `helpers/snippingtool` | Both launcher identities use Spectacle's icon instead of an Aero7-owned identity. |
| Aero7 File Explorer | `aero7-open-project/aero7-file-explorer` | The Dolphin-derived shell contains broad upstream theme lookup. Aero7-specific launcher and shell actions require namespaced resources; file, folder, MIME, device, and third-party service icons remain intentional external content. |
| Aero7 shell, taskbar, Start, tray, notifications, and SDDM integration | `aero7-open-project/aero7-desktop` plus the packaged AeroThemePlasma downstream | Shell-owned icons require stable namespaced assets. Icons supplied by running applications, StatusNotifierItem providers, MIME handlers, or notification senders remain external. |

## Direct findings

- Programs Center uses generic `go-previous`, `go-next`, `go-home`,
  `system-search`, `list-add`, `system-software-update`, and
  `applications-other` icons for owned UI.
- Control Panel uses generic launcher names (`preferences-system`, `flag`,
  `dialog-information`, and `system-software-install`) and routes its 45
  applets, eight categories, toolbar, statuses, actions, dialogs, and tray icon
  through the active theme.
- Device Manager routes its toolbar, device categories, states, properties,
  driver actions, and launcher/window identity through the active theme.
- Computer Management contains direct `QIcon::fromTheme` calls for the window,
  back/forward, tree, properties, refresh, help, navigation nodes, users/groups,
  Device Manager, and disk/optical entries.
- Gadget Gallery directly resolves the generic widget and web-browser icons.
- Internet Explorer resolves its own installed icon by theme name and uses
  generic icons for InPrivate, settings, and desktop actions.
- Snipping Tool identifies itself as Spectacle in both desktop files.

## Intentional exceptions

- Programs Center package cards and details may use the installed third-party
  application's icon, with an Aero7-owned package fallback.
- Control Panel Programs and Features may use third-party `.desktop` artwork.
- Default Programs and Internet Explorer may use the selected browser backend's
  actual icon.
- File Explorer may use MIME, user-content, mounted-device, and third-party
  service-menu icons.
- Notification and tray infrastructure may display artwork supplied by an
  external application, but Aero7-generated notifications and tray identities
  must use Aero7 assets.

## Implementation decision

At the project owner's direction, application-owned UI now uses selected PNG
assets from the same `aerothemeplasma-icons` pack already shipped by Aero7,
pinned to commit `96950b8028a5d960cb683280fe5f1d9e33e6b8a2`. The selected files are
embedded into each Qt target so changing the active icon theme cannot alter
them and a missing system theme cannot break the application. Launcher copies
are installed under stable `aero7-*` names in hicolor.

The upstream pack declares AGPL-3.0-or-later, and its README contains additional
Microsoft trademark and original-asset ownership notices. Both documents are
kept beside every imported asset set and installed with application
documentation. This provenance supersedes the initially planned procedural
artwork; none of that generated artwork remains in the converted components.

## Implemented source changes

| Surface | Result |
|---|---|
| Programs Center | Embedded pack actions, categories, package fallback, statuses, and navigation. Third-party package artwork remains external. |
| Control Panel suite | Embedded pack artwork for the 45-item catalog, eight categories, dialogs, statuses, Action Center, Getting Started, Personalization, Folder Options, Taskbar and Start Menu, update pages, and related tools. |
| Device Manager | Embedded pack artwork for device categories, device states, navigation, properties, driver actions, and window identity. |
| Computer Management | Embedded pack artwork for navigation, storage, services, accounts, Device Manager links, toolbar actions, and namespaced launcher aliases. |
| File Explorer | Replaced Aero7-owned Dolphin chrome and actions with embedded pack resources while retaining provider-owned file, MIME, location, device, and service icons. |
| Gadgets, Snipping Tool, and Internet Explorer | Embedded pack application/action/status icons and namespaced launcher or notification identities. The selected browser backend remains provider-owned. |

Launcher install rules use the pack's native small-size files wherever they
exist instead of scaling the 256px file at runtime. CMake installs every
available namespaced variant into its matching hicolor directory. This
includes 16–256px Control Panel identities, 16–256px Device Manager, distinct
Computer Management tool identities, 16–256px Gadget Gallery and Snipping
Tool identities, 16–256px Internet Explorer identities, and 16–256px File
Explorer identity files.

Static policy tests maintain explicit allowlists for remaining provider/content
lookups. Pixel tests switch among Aero7-like, Breeze, Breeze Dark, and missing
theme names and also exercise missing-resource fallback behavior.

## Test results

- 43 affected-component tests passed: Programs Center (3), Control Panel (12),
  Device Manager (4), Computer Management (9), Gadgets (3), both maintained
  Internet Explorer trees (4 each), Snipping Tool icon tests (2), and File
  Explorer icon tests (2).
- The Aero7 Desktop static, QML, script, desktop-file, configuration, session,
  migration, and source-policy suite passed 4/4.
- An asset-integrity comparison matched all 796 embedded `pack/` PNG files
  byte-for-byte to the pinned AeroThemePlasma icon repository. The 13 added
  Gadget Gallery and Snipping Tool native-size launcher copies also matched
  their pack sources byte-for-byte.
- The complete File Explorer/Dolphin-fork target tree builds. Its full CTest
  catalog passes 13/16; the three remaining failures are existing non-icon
  assertions in search-popup lazy loading, extended-attribute fallback, and
  main-window title/focus/accessibility behavior. The new File Explorer icon
  tests pass 2/2, and the inspected diffs in those failing paths only replace
  icon resolution.
- Only the affected Snipping Tool targets were built in the much larger
  AeroThemePlasma worktree; its two new icon tests pass.

## VM theme matrix

The rebuilt binaries were installed into the running Aero7 Wayland VM and
restarted after selecting each of these installed themes:

1. `Windows 7 Aero`
2. `breeze`
3. `breeze-dark`
4. `Adwaita`

Programs Center, Control Panel, Device Manager, and Computer Management were
opened after every change. Their application, toolbar, navigation, category,
status, and window artwork remained the embedded AeroThemePlasma pack artwork.
Third-party program cards remained intentionally provider-owned. Device
Manager's device context menu and Properties dialog and Computer Management's
Action menu were also exercised under Adwaita; their owned menu/dialog icons
continued to use the embedded pack.

The final File Explorer build was then installed and launched in the same VM.
Its `system-file-manager` application identity resolved directly from the
embedded pinned pack, the process remained running, and its runtime log
contained no missing Aero7 icon resource or pack warnings.

The 16 uncropped VM captures are indexed in
`docs/screenshots/icon-independence/README.md`. A namespaced Snipping Tool
notification was accepted by the VM's `org.freedesktop.Notifications` service,
but the running Plasma fallback did not render a toast during the capture
window. Notification source identity and embedded artwork pass automated
tests; visible toast artwork remains a live-session acceptance follow-up.

## Remaining boundaries

- Dynamic file, MIME, mounted-device, third-party application, browser-backend,
  service-menu, StatusNotifierItem-provider, and external-notification icons
  remain theme-aware by policy.
- The inactive native-shell prototype still contains generic QML icon names.
  Those names represent a mixture of external provider data and owned controls
  and need a separate runtime-shell resource pass before that prototype can
  replace the current Plasma fallback.
- No new replacement artwork should be drawn. Any future owned icon must be
  selected from the pinned AeroThemePlasma pack and recorded in provenance.
