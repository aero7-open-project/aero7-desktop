# Media Player

Media Player opens to a Windows 7-style music library with the Aero7 window
decoration. It is a native Aero7 app using VLC's playback engine, not the
ordinary VLC window. Open an MP3, M4A, MP4, or another supported file to enter
Now Playing, where album art or video, transport controls, seek, volume, and
full screen are available.

The library browses your Music and Videos folders. You can add a folder,
search, sort music by artist/album/genre, open individual files, or drag files
onto the window. Its Now Playing list can be saved and opened as an M3U
playlist. Album art and music tags are shown when the files provide them.

## Opening files

On first use, Aero7 selects Media Player for common audio and video types,
including MP3, M4A, and MP4, when there is no explicit user preference.
Existing file-opening choices are never overwritten. To change one, use the
file's **Open With** action or **Default Applications**.

The player uses icons bundled from the AeroThemePlasma icon pack. It uses
XWayland on Wayland sessions to embed libVLC 3 video within the Aero7-decorated
window. The old VLC skin is still available as a manual fallback, but the
normal **Media Player** launcher does not open VLC's own interface.

## What is not included

The library is a folder browser, not Windows Media Player's database. Burn,
Sync, CD ripping, Play To, DRM playback, and Windows taskbar thumbnail controls
are not implemented. Burn and Sync are visibly disabled.
