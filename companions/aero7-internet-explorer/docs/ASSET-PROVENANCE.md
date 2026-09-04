# Asset provenance

The namespaced application, InPrivate, settings, desktop-shortcut, and warning
PNG assets under `icons/` are selected from `aerothemeplasma-icons` commit
`96950b8028a5d960cb683280fe5f1d9e33e6b8a2`, the same pack revision pinned
by Aero7's Pacman repository.

The application master is `internet-web-browser.png`; 64 px and 128 px
launcher variants are mechanical downscales of the pack's 256 px file because
those sizes are not present upstream. The action and status files are unmodified
pack images renamed to stable Aero7 resource names.

Upstream declares AGPL-3.0-or-later. Preserve `icons/UPSTREAM-LICENSE` and
`icons/UPSTREAM-README.md`, including its attribution, trademark, and
original-asset ownership notices.

Core application, action, status, window, and notification artwork is embedded
with `resources/icons.qrc`. The selected third-party browser backend icon is
the only intentional theme-aware icon.
