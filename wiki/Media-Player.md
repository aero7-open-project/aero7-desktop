# Media Player

Aero7's Media Player uses VLC's playback engine with a Windows 7-inspired
skin. The dark Now Playing window includes album art or video, a seek bar,
play/pause, stop, previous/next, volume, mute, full screen, and file opening.
The list button opens a separate, working VLC playlist window. Files may also
be dragged onto either window.

The player is included in the `aero7-desktop` package; `vlc`,
`vlc-gui-skins2`, and `vlc-plugin-ffmpeg` are required dependencies. Its application and control icons
are bundled from the AeroThemePlasma icon pack, so changing the global icon
theme does not change the player identity.

## Opening files

On first use, Aero7 selects Media Player for common audio and video types,
including MP3, M4A, and MP4, when there is no explicit user preference.
Existing file-opening choices are never overwritten. To change one, use the
file's **Open With** action or **Default Applications**.

If a file opens in standard VLC instead, check that you launched **Media
Player** and that `vlc-gui-skins2` is installed. If a video opens outside the
skinned window, ensure VLC's **skinned video** setting is enabled.

## What is not included

This is a real VLC skin, not a port of Windows Media Player. A skin cannot
add Windows Media Player's indexed/searchable media library, Burn and Sync
tabs, CD ripping interface, Play To device routing, or Windows taskbar
thumbnail controls. The playlist is a real queue, not a fake library.
