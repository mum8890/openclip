# OpenClip

Published by **Ghostreel**. An open-source video editor written from scratch in Qt 6 / C++ (not affiliated with or derived from CapCut).

## Build
Requires Qt 6 (Widgets, Multimedia), CMake 3.16+, and `ffmpeg` + `ffprobe` on PATH.

    cmake -B build
    cmake --build build
    ./build/openclip

## Current features
- Pro-style workspace: dockable panels, workspaces, full menu bar
- Save/Open projects (.openclip), unlimited-style Undo/Redo
- Editor-style interface: media panel, preview, inspector and timeline
- Add clips (with or without audio), live preview, visual timeline, zoom (Ctrl+wheel)
- Split (S), delete (Del), play/pause (Space), per-clip speed 0.5x-2x
- Live preview of the finished result: pause or scrub for a real frame with effects, transitions and titles, play for a background-rendered preview ("FX preview" switch for slower PCs)
- Effects: 25 looks (Black & White, Vivid, Warm, Cool, Cinematic, Sepia, Blur, Sharpen, Noir, Golden Hour, Teal & Orange, Cyberpunk, Cold Night, Vintage, Faded Film, Punch, Dream, Old Film, Invert, Mirror, Sketch, Infrared, Killcam Red, Night Vision, HDR Boost) plus brightness/contrast/saturation
- Transitions: 51 (fade, dissolve, wipes, slides, smooth, circle, rectangle, diagonals, slices, pixelize, radial, zoom, blur, squeeze, cover, reveal) that overlap the clips and shorten the project, just like the exported file
- Text overlays: 6 styles (Gaming, Yellow Outline, Neon, Red Alert, Boxed, Plain), position, fade / slide-up animation, also shown in the live preview
- Export: 480p to 8K, 24/30/60 fps, 16:9 / 9:16 / 1:1, fit or fill, quality + speed options, with a large progress window (percentage, time remaining, Cancel)
- Upscaling for low-quality footage: Lanczos, + sharpen, + noise clean (classic scaling, not AI)
- Templates: Gameplay Highlight, Vertical Short, Kill Montage, Cinematic Trailer. Templates are plain .json files in resources/templates, so anyone can write and share new ones
- Automatic update notifications through GitHub Releases
- Quick preview: renders a small version with all effects, transitions and text, then opens it

## Roadmap
0. (done in 0.6) dockable workspace, menus, project files, undo/redo
0. (done in 0.7) live preview of effects/transitions/titles, export progress window, 25 effects + 51 transitions
1. Trim handles, drag to reorder, undo/redo, multiple tracks
2. Music / audio track, volume, fade in/out
3. Keyframes (zoom/pan), stickers, picture-in-picture
4. Whisper auto-captions, project save/load
5. More templates + a template gallery
6. Optional AI upscaling (Real-ESRGAN)

License: GPL-3.0 (see LICENSE). The installer bundles a GPL build of FFmpeg.

## Releasing a new version (auto-update)
1. Upload the changed files to GitHub.
2. GitHub -> Releases -> Draft a new release -> Choose a tag: type e.g. `v0.7.0` -> Create new tag.
3. Title: `OpenClip 0.7.0`. Description: what's new (users see this in the update window).
4. Publish release. After ~15 minutes the installer appears on that release, and OpenClip
   offers the update to everyone on next start (or via Help -> Check for updates).

## Download
Latest installer: https://github.com/mum8890/openclip/releases/latest
