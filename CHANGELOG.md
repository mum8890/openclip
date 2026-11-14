# Changelog

## 0.8.0 - Drag, drop, and ask
- Drag and drop: drag a look from the Effects panel onto any clip, or a transition from the Transitions panel onto a cut. The timeline now shows them as their own items: a purple **FX** block under each clip that has a look, and a transition block straddling the join. Drag a transition block's edge to change its length (0.1-2.0 s), click it to preview the blend, right-click to remove it. Everything is undoable
- 10 new looks (35 in total): Neon Glow, VHS, Blocky, Chromatic, Frost, Moody, Sunkissed, Twilight, Bleach, Lomo. Each chain was tested against the bundled FFmpeg
- AI assistant, built in: ask in plain words and the agent adds clips, looks, transitions, titles, cuts, speed and colour changes, then shows the result. It edits only the timeline and keeps a full undo history
- Free AI options: Ollama running on your own PC (no account, no key), or free API keys from Groq, Google Gemini and OpenRouter. OpenAI and any custom OpenAI-compatible server also work
- "Ask before changes" switch: review every action the assistant wants to take before it touches the timeline. The API key stays on your PC and only timeline details are sent, never your video files
- New interface: aurora gradient window, frosted translucent panels, colourful cyan/violet/pink accents and a dark rounded Windows 11 title bar
- New AI menu (Ctrl+Shift+A) and an "AI" workspace; the assistant dock tabs with Properties

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
