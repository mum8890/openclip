# Changelog

## 0.7.0 - See it before you export
- The preview now shows the real thing: pause or scrub and a frame with every effect, transition blend and title is drawn under the playhead; press Play and OpenClip renders a small preview in the background and plays that instead of the raw clips
- "FX preview" switch in the player bar and the View menu, for slower computers
- New export window with a big percentage, progress bar, elapsed time, time remaining and a working Cancel button (also used by Quick preview)
- More looks: 25 effects in total (Noir, Golden Hour, Teal & Orange, Cyberpunk, Cold Night, Vintage, Faded Film, Punch, Dream, Old Film, Invert, Mirror, Sketch, Infrared, Killcam Red, Night Vision, HDR Boost and the original eight)
- More transitions: 51 in total, covering the whole FFmpeg xfade set (wipes, slides, smooth, circle, rectangle, diagonals, slices, pixelize, radial, zoom, blur, squeeze, cover, reveal and fades), with friendly names in the menus
- The timeline and the exported video now share one clock: transitions overlap the clips and shorten the project instead of shifting everything after them, so the playhead lines up with the final file
- Splitting inside a transition is prevented, and transition-aware seeking means playback switches clips at the right moment

## 0.6.0 - Pro workspace
- Dockable panels (Project, Effects, Transitions, Titles, Templates, Properties, Timeline): drag, tab, float, close
- Workspaces (Window menu): Editing, Assembly, Effects, Titles + Reset to Default Layout; your layout is remembered
- Full menu bar: File, Edit, Clip, Sequence, Graphics and Titles, View, Window, Help (with keyboard shortcuts)
- Save / Open projects (.openclip files), with a warning if videos were moved
- Undo / Redo (Ctrl+Z / Ctrl+Y) for every edit; slider drags count as one step

## 0.5.0
- Brand-new interface: dark theme, media library + tool tabs on the left, live inspector on the right,
  player bar with seek slider, and a redesigned timeline with lanes, zoom slider and transition markers
- Import media once, then double-click to add clips to the timeline
- Click an effect, transition or text style to apply it; fine-tune everything in the inspector
- Friendly first-launch screen and helpful status-bar hints

## 0.4.0
- New app icon and window/installer branding
- Export up to 4K and 8K, 24/30/60 fps, 16:9 / 9:16 / 1:1
- Upscaling for low-quality footage (Lanczos, sharpen, noise clean)
- Effects (8 looks + brightness/contrast/saturation) and 14 transitions
- Text overlays with 6 styles and animations
- Templates: Gameplay Highlight, Vertical Short, Kill Montage, Cinematic Trailer (+ open your own .json)
- Quick preview, speed control (0.5x-2x), split/delete/play shortcuts
- Automatic update notifications (Help -> Check for updates)

## 0.3.0 and earlier
- First versions: timeline, preview player, split, speed, MP4 export
