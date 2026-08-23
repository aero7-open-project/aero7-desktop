# Existing Component Audit

Audit date: 2026-08-16. Sources were inspected from the local Aero7 project
worktrees and the pinned metadata in `memegeko/aero7-repo`. The retired
installer/script repository was explicitly excluded from implementation use.

| Component | Repository/source | Current role | Framework | Plasma/KWin dependency | Wayland | Decision | Licence | Maintenance risk | Aero7 integration point |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Aero7 OS/installer | `aero7-open-project/aero7` | Arch ISO, installer, OOBE, focused package lists | Qt 6/QML, Python | Installs Plasma 6/KWin | Required | Adapt package/session lists after testing | MIT; third-party assets separate | Medium: installer and rolling Arch move together | Add `aero7-desktop` only on testing branch first |
| Aero7 package repository | `memegeko/aero7-repo` | Clean-chroot build, signing, package manifests | Bash, Python, PKGBUILD | Builds AeroShell stack | Required | Reuse build/signing infrastructure unchanged; add new recipe | MIT for infrastructure; per-package licences | Medium: VCS pins need continuous rebuild testing | New `aero7-desktop` package after VM pass |
| AeroShell libplasma | pinned `aeroshell-libplasma-git` | Patched libplasma provider | C++/Qt/KF | Replaces libplasma | Yes in current package set | Reuse during compatibility phase | LGPL-2.0-or-later | High: deep Plasma fork | Runtime compatibility dependency; review removal later |
| AeroShell workspace | pinned `aeroshell-workspace-git` | Start/taskbar/workspace support | C++/Qt/KF | Plasma workspace; libplasma | Yes | Reuse as the visible normal shell at the tested pin | AGPL-3.0-or-later | Medium/high: follows Plasma internals | Session shell |
| AeroShell KWin components | pinned `aeroshell-kwin-components-git` | Switchers/effects/decor integration | C++/QML/KWin | KWin | Yes | Reuse supported components; avoid new KWin fork | Upstream package metadata | Medium | Window experience compatibility |
| AeroThemePlasma desktop | `memegeko/aerothemeplasma`, pinned package | Integrated theme, shell layout, search patches | C++/QML/Plasma | Broad Plasma and KWin runtime | Yes | Reuse after applying the Plasma 6.7 containment-order fix and clean-login tests | AGPL-3.0-or-later | High: broad dependency surface | Visible Aero desktop, panel and Start |
| Aero icons | pinned `aerothemeplasma-icons-git` | Icon theme | Data assets | None at runtime beyond icon loaders | Yes | Reuse unchanged subject to asset audit | Package/upstream licence | Low/medium | Default icon theme |
| Aero sounds | pinned `aerothemeplasma-sounds-git` | Sound theme | Data assets | Plasma notifications currently | Yes | Reuse if provenance remains acceptable | Package/upstream licence | Medium: asset provenance | Default sound theme |
| SMod | pinned `aeroshell-smod-git` | Window decoration/effects | C++/KWin | Direct KWin dependency | Yes package variant | Reuse only at tested pin | Package/upstream licence | High: KWin ABI churn | Decorations/effects |
| UAC polkit agent | pinned `uac-polkit-agent-git` | Branded authentication agent | Qt/Polkit | No required Plasma UI | Yes | Reuse after single-agent conflict test | Package/upstream licence | Medium | Session authentication agent |
| Aero7 Control Panel | `aero7-open-project/aero7-control-panel-` | Normal settings UI and native Linux backends | Qt 6 Widgets, D-Bus | Some compatibility commands target Plasma | Yes | Reuse and extend; do not merge | GPL-3.0 | Medium | `control` and `control --setting ...` |
| Device Manager | `aero7-open-project/aero7-device-manager` | Real sysfs/procfs device catalog and actions | Qt 6 Widgets/D-Bus | Theme only | Yes | Reuse; preserve current uncommitted local work | GPL-3.0 | Medium: privileged device actions | `aero7-device-manager --open ...` |
| Computer Management | `aero7-open-project/aero7-computer-management` | Real systemd/journal/UDisks/admin console | Qt 6 Widgets/D-Bus | Theme only | Yes | Reuse unchanged as separate application | MIT | Medium/high: destructive storage features | `aero7-computer-management` deep links |
| Programs Center | `aero7-open-project/aero7-programs-center` | libalpm/AppStream software UI | Qt 6 Widgets, libalpm | Theme only | Yes | Integrate as optional until alpha exit | MIT | High: privileged package transactions | Programs/default-program links |
| Aero7 Qt | local package recipe in `aero7-repo` | Shared visual widgets/components | Qt 6 | Theme integration | Yes | Reuse as app dependency; do not vendor | Package source metadata incomplete | Medium | Consistent app styling |
| Aero7 Dolphin | pinned downstream in `aero7-repo` | File Explorer | Qt/KDE/KIO | KDE Frameworks, not shell | Yes | Keep separate and reuse | GPL family/upstream Dolphin | High: downstream application fork | Computer/file links |
| Aero7 Gwenview | pinned downstream in `aero7-repo` | Photo Viewer | Qt/KDE | KDE Frameworks | Yes | Keep separate and reuse | GPL family/upstream Gwenview | High: downstream application fork | Picture/photo associations |
| Aero7 KolourPaint | pinned downstream in `aero7-repo` | Paint application | Qt/KDE plus SARibbon | KDE Frameworks | Yes | Keep separate and reuse | Upstream licences | High: multiple forks | Paint launcher |
| Aero7 Gadgets | local recipe/components in `aero7-repo` | Clock, CPU, Notes concepts | QML/Plasma | Plasma containment | Yes | Keep optional and reuse through the Aero desktop containment | MIT | Low/medium | Optional AeroShell widgets |
| execbin | pinned companion | Executable compatibility helper | Native utility | None documented | Yes | Keep separate; optional integration only | Upstream metadata | Medium | File launch compatibility |
| LinVer | pinned companion | Version/system information | Qt | Aero7 Qt | Yes | Reuse | Upstream metadata | Medium | System information page |
| TuxManager | pinned companion | Task Manager-like application | Qt/native Linux APIs | None documented | Yes | Reuse if runtime validation passes | Upstream metadata | Medium | Task Manager link |
| WinXplorer | pinned but omitted by current ISO | Alternative explorer | Qt | Unknown | Unknown | Do not default; Aero7 Dolphin is established | Upstream metadata | High | None by default |
| Sevulet | referenced but not packaged | Companion application | Unknown | Unknown | Unknown | Do not use: source/licence unavailable | Unverified | Unacceptable | None |

## Reuse conclusion

The corrected architecture keeps Plasma/KWin/KF6 and the existing AeroShell
components in their intended roles. This repository owns session policy,
deterministic layout, migration, recovery, packaging, and integration—not a
second overlay shell or settings hub. SevenStart's Plasma 6.7 containment-order
crash is fixed at its source package, and a stock panel is not used as fallback.
Separate Aero7 applications remain separate repositories. No source from the
excluded legacy script project is imported or executed.
