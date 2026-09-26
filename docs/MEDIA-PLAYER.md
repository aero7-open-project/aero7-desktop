# Aero7 Media Player

Aero7 Media Player is a VLC skins2 player styled after Windows Media Player
12's separate Now Playing window. It retains VLC's real codec and playback
engine. The package requires `vlc`, `vlc-gui-skins2`, and
`vlc-plugin-ffmpeg` (needed for common H.264/MP4 video on Arch's split VLC).

## What works

- MP3, M4A, MP4 and other formats supported by the installed VLC plugins.
- Embedded video, album art (when present), current title and time.
- Play/pause, stop, previous/next, seek, volume, mute, full screen, open file,
  drag-and-drop, and a separate, interactive playlist window.
- File-opening from Explorer and other XDG-compliant apps through the
  `aero7-media-player.desktop` MIME associations.

Aero7 adds media defaults to the user's `mimeapps.list` only where that user
has not already chosen a handler. To change an association later, use Default
Applications or the file's Open With dialog. Existing choices are not reset.
The exact older Aero7-generated `vlc.desktop` override is hidden to avoid a
second identically named Start entry; a `.aero7-pre-skin` backup is retained
beside it. Customized VLC launchers are not changed.

## Limits

VLC skins2 cannot implement Windows Media Player's indexed/searchable media
library, Burn and Sync tabs, CD ripping workflow, Play To device routing, or
Windows taskbar thumbnail controls. The playlist view is a real VLC queue,
not an inactive imitation of those features. Some audio/video formats require
additional VLC codec plugins or have DRM that VLC cannot play.

## Troubleshooting

- If the player opens in ordinary VLC chrome, verify `vlc-gui-skins2` is
  installed and launch `aero7-media-player.desktop` rather than `vlc.desktop`.
- If files still open in another app, check `xdg-mime query default audio/mpeg`,
  `audio/x-m4a`, or `video/mp4`. An explicit earlier choice is intentionally
  preserved; choose Media Player in Default Applications to change it.
- If video opens separately, check VLC's `skinned-video` setting is enabled.
  It is enabled by default in VLC 3.0.23.

The underlying skin file is `/usr/share/vlc/skins2/aero7-media-player.vlt`.
The launcher and native-size icon resources are installed by `aero7-desktop`.
