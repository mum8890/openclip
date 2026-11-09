# OpenClip

An open-source video editor written from scratch in Qt 6 / C++ (not affiliated with or derived from CapCut).

## Build
Requires Qt 6 (Widgets, Multimedia), CMake 3.16+, and `ffmpeg` + `ffprobe` on PATH.

    cmake -B build
    cmake --build build
    ./build/openclip

## Current features
- Add clips (with or without audio), live preview player, click/drag timeline with playhead, Ctrl+wheel zoom
- Split at playhead (S), delete (Del), play/pause (Space)
- Per-clip speed (0.5x-2x) for slow-mo and fast-forward
- Export MP4 at 1080p, 24/30/60 fps (H.264/AAC)

## Roadmap
1. Trim handles on clip edges, drag to reorder, multiple tracks
2. Undo/redo (QUndoStack), text overlays, zoom/crop/rotate, volume
3. Filters, transitions, keyframes
4. Whisper auto-captions, project save/load
5. Gaming extras: audio-peak "highlight" detection, vertical (9:16) export for Shorts/TikTok

Pick a license before publishing (GPL-3.0 and MIT are common; FFmpeg builds with GPL parts push you toward GPL).
