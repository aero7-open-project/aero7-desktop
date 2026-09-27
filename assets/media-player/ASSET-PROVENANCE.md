# Media Player asset provenance

The original skins2 fallback uses Aero7 `main.svg`, `playlist.svg`, and
`slider.svg` backdrops rendered to matching PNG files. The default native
window draws its library chrome in Qt and uses the active KWin decoration.
No replacement control or application icon was drawn for this work.

All files under `icons/` were copied without pixel edits from the installed
`aerothemeplasma-icons-git` package, version `11.r96950b8-3`, corresponding
to the approved icon-pack commit `96950b8028a5d960cb683280fe5f1d9e33e6b8a2`.
Control icons come from its `actions/32`, `actions/16`, and `status/16`
directories. The native library's `icons/library/` folder additionally uses
unmodified `actions/32`, `places/22`, and `mimetypes/256/sound.png` assets.
The fallback skin's large placeholder and app launchers come from
`apps/256/audio-player.png` and the native-size `apps/{16,24,32,48,64,256}`
variants respectively. Files were only renamed for embedding and hicolor
installation; no resize was applied to the launcher icons.

`ICON-LICENSE` and `ICON-PROVENANCE.md` are the unchanged license and README
distributed with that icon package. The app's MIT source license does not
relicense the bundled icons.
