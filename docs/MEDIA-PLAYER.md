# Aero7 Media Player

Aero7 Media Player is a native Qt window with the active Aero7/KWin window
decoration. It uses libVLC for playback, so it is not the standard VLC window.
Opening it without a file shows a Windows 7-style library; opening a media file
switches to Now Playing. The earlier VLC skins2 theme remains packaged as an
optional fallback but is not the application launcher.

## Library and playback

- The Music and Videos folders are scanned in the background. **Add folder**
  adds another location without changing or moving files; **Refresh library**
  rescans them. A search box filters title, artist, album, and genre.
- Music tags, track length, and embedded or folder cover art are read when
  available. Artist, Album, and Genre entries sort the same real library.
- Double-click a track to play it. **Open file** and drag-and-drop also work.
  The Now Playing list can be opened, saved as M3U, and loaded from M3U.
- Play/pause, stop, previous/next, seek, volume, mute, and full screen control
  libVLC. MP3, M4A, and MP4 are tested formats; additional formats depend on
  the installed VLC plugins.
- A network stream can be opened from the **Stream** menu. This uses libVLC's
  URL playback; supported protocols vary by installation.

On a Wayland desktop, the app uses XWayland so libVLC 3 can embed video in its
decorated window. The package depends on `xorg-xwayland`, `libvlc`, `taglib`,
and VLC's FFmpeg plugin. The icons are embedded from the AeroThemePlasma icon
pack, so changing the global icon theme does not change this app's controls.

Aero7 adds media defaults to the user's `mimeapps.list` only where that user
has not already chosen a handler. To change an association later, use Default
Applications or the file's Open With dialog. Existing choices are not reset.
The exact older Aero7-generated `vlc.desktop` override is hidden to avoid a
second identically named Start entry; a `.aero7-pre-skin` backup is retained
beside it. Customized VLC launchers are not changed.

## Limits

The library browses folders; it is not Windows Media Player's indexed media
database or online media guide. Burn, Sync, CD ripping, Play To, and Windows
taskbar thumbnail controls are not implemented. Burn and Sync tabs are shown
disabled, rather than pretending to work. DRM-protected Windows media may not
play through libVLC.

## Troubleshooting

- If **Media Player** still opens the ordinary VLC window, check which entry
  was launched. The Aero7 desktop file is
  `/usr/share/applications/aero7-media-player.desktop` and runs
  `/usr/bin/aero7-media-player`, not `vlc.desktop`.
- If files still open in another app, check `xdg-mime query default audio/mpeg`,
  `audio/x-m4a`, or `video/mp4`. An explicit earlier choice is intentionally
  preserved; choose Media Player in Default Applications to change it.
- If embedded video is missing, check that XWayland is installed and `DISPLAY`
  exists in the session. The media engine can still play audio without it.

The older manual skin remains at `/usr/share/vlc/skins2/aero7-media-player.vlt`.
