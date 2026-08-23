# Aero7 asset map

This map establishes the Phase 0 asset priority. Installed theme assets are
referenced in place. ISO assets are copied only when the installed theme does
not provide the required desktop default.

| Purpose | Source repository/package | Source path | Installed/runtime path | Licence status | Consumer |
| --- | --- | --- | --- | --- | --- |
| Desktop wallpaper | local Aero7 ISO workcopy | `installer/assets/aero7-background.png` | `/usr/share/wallpapers/Aero7/contents/images/1672x941.png` | Aero7 ISO project is MIT; README notes individual artwork may retain separate terms, so provenance must remain recorded | AeroTheme desktop containment; clean blue target background without an added center logo |
| Branded background variant | local Aero7 ISO workcopy | `installer/assets/aero-shell/aero7-background.png` | retained only in the source project that owns it | same repository notice as above | login/branding contexts, not the clean desktop default |
| Aero7 circle logo | local Aero7 ISO workcopy | `installer/assets/aero7-logo-circle.png` | `/usr/share/aero7-desktop/branding/aero7-logo-circle.png` | same repository notice as above | About/login branding |
| Aero7 plain logo | local Aero7 ISO workcopy | `installer/assets/aero7-logo-plain.png` | `/usr/share/aero7-desktop/branding/aero7-logo-plain.png` | same repository notice as above | About/recovery branding |
| User image | local Aero7 ISO workcopy | `installer/assets/aero-shell/aero7-user.png` | prefer AccountsService user icon; packaged fallback under Aero7 branding | same repository notice as above | SevenStart/login fallback |
| Start orb | `aerothemeplasma-desktop-git` | `plasma/plasmoids/io.gitgud.wackyideas.SevenStart/contents/ui/orbs/` | `/usr/share/plasma/plasmoids/io.gitgud.wackyideas.SevenStart/contents/ui/orbs/` | AGPL-3.0-or-later package | SevenStart |
| Start menu frames/buttons | `aerothemeplasma-desktop-git` | `SevenStart/contents/ui/svgs/` | matching `/usr/share/plasma/plasmoids/...` paths | AGPL-3.0-or-later package | SevenStart |
| Taskbar glass and task states | `aerothemeplasma-desktop-git` | `plasma/desktoptheme/Seven-Black/widgets/` and `seventasks/contents/ui/svgs/` | `/usr/share/plasma/desktoptheme/Seven-Black/` and SevenTasks plasmoid path | AGPL-3.0-or-later package | Aero panel and SevenTasks |
| System tray graphics | `aerothemeplasma-desktop-git` | `systemtray/contents/ui/svgs/` | `/usr/share/plasma/plasmoids/io.gitgud.wackyideas.systemtray/` | AGPL-3.0-or-later package | Aero system tray |
| Window decoration | `aeroshell-smod-git` plus Aero7 ISO reference | SMod source; `installer/assets/smod/` for reference | `/usr/lib/qt6/plugins/org.smod.smod.so` and SMod decoration assets | AGPL-3.0-or-later package; ISO notice for reference images | KWin/SMod |
| Application and desktop icons | `aerothemeplasma-icons-git` | package theme tree | `/usr/share/icons/Windows 7 Aero` | AGPL-3.0-or-later package | Qt/KDE apps, desktop, Start, taskbar |
| Cursor theme | `aerothemeplasma-icons-git` | package cursor tree | `/usr/share/icons/aero-drop` | AGPL-3.0-or-later package | KWin/Qt cursor |
| Default sound theme | `aerothemeplasma-sounds-git` | package sound tree | `/usr/share/sounds/Aero7` | AGPL-3.0-or-later package | KNotifications/desktop events |
| Alternate sound themes | `aerothemeplasma-sounds-git` | package sound trees | `/usr/share/sounds/Aero7 *` | AGPL-3.0-or-later package | Personalization |
| Control Panel category icons | `linux-control-panel` and active icon theme | `aero7-control-panel-` local workcopy plus themed icon names | application binary and `/usr/share/icons/Windows 7 Aero` | GPL-3.0 for Control Panel; icon package licence above | real Aero7 Control Panel |
| File Explorer icons | `aero7-file-explorer` and active icon theme | separate package | package paths plus `/usr/share/icons/Windows 7 Aero` | package licences | Aero7 File Explorer |

## Fallback policy

The normal session sets `Windows 7 Aero`, `aero-drop`, `Aero7`, the Aero7
wallpaper, `Seven-Black`/Aero7 Plasma theme assets, and SMod explicitly. Breeze
is permitted only for an icon or control not supplied by Aero7/AeroTheme and
must be recorded here before a visible fallback is accepted.
