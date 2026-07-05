# GSplus Backlog

Planned and possible work for GSplus. Everything below targets the SDL3 build
(the focus — see the README). Items marked **(core)** already exist in the KEGS
core and mainly need wiring into the SDL driver; everything else is new.

Effort: **S** small · **M** medium · **L** large. Priority: High / Med / Low.

---

## Done

- Foundation: SDL3 driver (video/input/audio), CMake build, cross-platform CI,
  self-contained packaging (macOS `.dmg`, Linux `.tar.gz`, Windows `.zip`),
  version-from-tag.
- **F1 — Window & display options**: `-fullscreen`, `-borderless`, `-noaspect`,
  `-highdpi`, `-nohwaccel`, window position; F11 fullscreen toggle. (`-novsync`
  was later removed — see the vsync item under Display.)
- **F2 — Scanline simulator**: `-scanline <0-100>`, Shift+F11 toggle.
- **F3 — Drag-and-drop**: drop a disk image on the window to mount it (slot
  guessed from file size).
- **Curved CRT effect**: `-crt 1` + `-crtcurve <0-100>` + `-crtmask <0-100>`,
  Ctrl+F11 toggle. Curvature (RenderGeometry mesh) + RGB phosphor mask + bloom +
  vignette, composes with `-scanline`. All on the 2D renderer, no extra libraries.
- **Gamepad/controller support**: SDL3 high-level Gamepad API → IIgs
  joystick/paddles, with hotplug. Select "Native Joystick 1" in the F4 config
  menu to route paddles to it.
- **Screenshots**: F10 captures the framebuffer (`-ssdir` sets the output
  directory).

---

## Legacy gsplus features (original roadmap)

| Item | Pri | Eff | Notes |
|---|---|---|---|
| **Clipboard** copy/paste | Med | M | Wire SDL clipboard to the core's copy/paste hooks **(core)**. |

## Input

| Item | Pri | Eff | Notes |
|---|---|---|---|
| **Mouse capture** toggle | Med | S | Grab/release the pointer via a hotkey. |
| Configurable **key remapping** | Low | M | Currently a fixed scancode→ADB table. |
| **Gamepad button remapping** | Low | S | SDL_Gamepad's stable button names make a small remap table in the config menu cheap. |

## Display

| Item | Pri | Eff | Notes |
|---|---|---|---|
| **Integer / pixel-perfect scaling** toggle | Med | S | Crisp 1×/2×/3× vs. letterbox stretch; also stops scanline shimmer at non-integer scales. |
| Linear-filter option | Low | S | Smooth-scaling alternative to nearest. |
| Re-add **vsync** option properly | Low | M | Removed pre-1.38.0 (double-pacing caused half-speed + Windows audio gaps). Right fix: when vsync is on, let it drive pacing and drop the sleep-based clock — one pacer, not two. |

## Runtime / system

| Item | Pri | Eff | Notes |
|---|---|---|---|
| **Fast-forward / turbo** hotkey | High | S | Hold to uncap speed. |
| **Pause/resume** hotkey | Med | S | |
| **Save states / snapshots** | Med | L | Full IIgs machine state; KEGS has none. Stretch. |

## Audio

| Item | Pri | Eff | Notes |
|---|---|---|---|
| **Volume / mute** hotkey | Med | S | |
| Audio device selection | Low | S | |

## Quality of life

| Item | Pri | Eff | Notes |
|---|---|---|---|
| **Recent disks/ROMs** menu | Med | M | |
| Window title shows **mounted disk** | Low | S | |
| Modern **overlay config menu** (vs. F4 text panel) | Low | L | Big polish, big effort. |

## Packaging / distribution

| Item | Pri | Eff | Notes |
|---|---|---|---|
| **Linux AppImage** (self-contained) | Med | M | The one non-self-contained download today. |
| **Code signing** (Windows/macOS) | Low | M | Needs a cert / Azure Trusted Signing. |
| Local **mingw cross-compile** for fast Windows iteration | Low | S | Catch Win compile/link errors on macOS in seconds. |

## Testing / infrastructure

| Item | Pri | Eff | Notes |
|---|---|---|---|
| **CI smoke test** | High | M | Headless boot (SDL `dummy` video driver) for N emulated frames, checksum the framebuffer. Catches CPU/memory/video regressions on every PR — highest-leverage single test, especially for upstream KEGS merges. Needs a redistributable boot image or ROM-free path. |

## Upstream (offer back to KEGS)

Per policy: core (non-SDL) fixes get offered upstream. Current candidates,
verified clean by review:

- **VOC SHR shadow fix** — `moremem.c` `fixup_ramwrt()`, bank $E0 pages
  $6000-$9FFF. Reword the misleading "bank 0" comment first.
- **Windows timer fix** — `clock.c` `timeBeginPeriod(1)` + Sleep rounding;
  winmm is already linked in upstream's vcxproj.
- **dynapro.c dirent guard** — `_WIN32 && _MSC_VER` check; benefits any
  mingw build of upstream.

---

## Suggested order

Fast-forward → Clipboard.

## Dagen's wishlist

_(personal "selfish feature" ideas — add freely)_
