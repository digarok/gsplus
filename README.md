![GSplus - cross-platform Apple IIgs emulator](gsplus-social.png)

# GSplus

An accessible, cross-platform build of [KEGS](http://kegs.sourceforge.net) - 
Kent Dickey's Apple IIgs emulator - packaged with
[SDL3](https://www.libsdl.org/) for macOS, Linux, and Windows.

## What it is

GSplus wraps the KEGS core in a single SDL3 application that builds and runs the
same way on macOS, Linux, and Windows. It is **focused on the SDL3 build** - if
you want the native macOS or Windows versions, use
[KEGS](http://kegs.sourceforge.net) directly. GSplus tracks the KEGS version it's
based on (currently 1.38).

<a href="gsplus-screenshot-mac.png">
  <img src="gsplus-screenshot-mac.png" alt="GSplus running on macOS" width="400">
</a>

## You'll need a ROM

Like any IIgs emulator, GSplus needs an Apple IIgs ROM file to boot. Without one
it starts into a configuration screen with nothing to run. The ROM is Apple's
copyrighted firmware, so GSplus can't ship it — you supply your own.

**Which ROM:** the IIgs shipped with two ROM versions. **ROM 03** (256 KB, the
later model) is the usual choice; **ROM 01** (128 KB) is also fully supported.
GSplus does **not** support the early ROM 0.

**Where to put it:** name the file `ROM`, `ROM.01`, or `ROM.03` and place it in
your home directory, next to the app, or next to your `config.kegs`. GSplus picks
it up automatically. You can also select one at runtime: press **F4 → ROM File
Selection → ROM File** and browse to it. A handful of other common names
(`APPLE2GS.ROM`, `APPLE2GS.ROM2`, `xgs.rom`, `342-0077-b`, …) are recognized too.

**Finding one:** the cleanest option is to dump the ROM from a real Apple IIgs you
own — KEGS includes a short BASIC program that does it, in
[`upstream/kegs/doc/README.ROM.files.txt`](upstream/kegs/doc/README.ROM.files.txt).
Otherwise, the IIgs ROM is long-unsupported software that's widely circulated
online: a web search for *"Apple IIgs ROM 03"* (or ROM 01) generally turns one
up, and the Internet Archive hosts MAME sets that contain a usable ROM. That same
`README.ROM.files.txt` lists the exact filenames KEGS accepts and how to extract
or convert a MAME-format dump.

**Did it work?** A valid ROM 01 file is exactly **128 KB** and a ROM 03 is **256
KB**. If GSplus boots to the "Check startup device" screen, the ROM loaded fine —
it just needs a disk. Press **F4** to mount a disk image, or drag one onto the
window.

## Downloads

Prebuilt apps are attached to each [release](../../releases). The builds aren't
code-signed yet, so your OS may warn that the app is from an unidentified
developer the first time you run it. This is expected:

**Windows** - SmartScreen blocks unsigned downloads. Click **More info → Run
anyway**. If the `.exe` won't start after unzipping, right-click it →
**Properties** → tick **Unblock** → **OK**. Keep `SDL3.dll` next to the `.exe`.

**macOS** - the app is only ad-hoc signed, so Gatekeeper flags it. Right-click
the app → **Open**, then confirm **Open**. If macOS still refuses, run
`xattr -dr com.apple.quarantine /Applications/GSplus.app`.

**Linux** - the `.tar.gz` relies on a system SDL3 (`libsdl3` from your package
manager). A self-contained AppImage is planned.

## Status

This is an early reboot - expect rough edges. SDL3 **video, input, and audio**
are working on macOS, Linux, and Windows.

### What's been added in GSplus

Some features that are a part of the SDL3 build:

- **Window & display options** - `-fullscreen`, `-borderless`, `-noaspect`,
  `-highdpi`, `-novsync`, `-nohwaccel`, plus window position; F11 toggles
  fullscreen.
- **Screenshot capture** - F10 writes the framebuffer to disk.
- **Gamepad support** - cross-platform SDL_Gamepad mapped to the IIgs joystick.
- **Terminal debugger** - a REPL for the built-in 65816 debugger from the
  controlling terminal.
- **Drag-and-drop disk loading** - drop a disk image on the window and GSplus
  guesses the slot from its size.
- **CRT scanline simulator** - `-scanline <0-100>`, Shift+F11 to toggle.
- **Curved CRT effect** - `-crt 1` with `-crtcurve` / `-crtmask`, Ctrl+F11 to
  toggle. Screen curvature, RGB phosphor mask, bloom, and vignette; composes
  with the scanline simulator.
- **One cross-platform build** - the same SDL3 app and CMake build on macOS,
  Linux, and Windows, with prebuilt downloads for each.

## Controls

**Additional GSplus hotkeys** — handled by the app (the SDL build), not sent to the
emulated IIgs:

| Key | Action |
|---|---|
| **F10** | Save a screenshot |
| **F11** | Toggle fullscreen |
| **Shift + F11** | Toggle the CRT scanline effect |
| **Ctrl + F11** | Toggle the CRT curved screen effect |
| **Drag & drop** | Drop a disk image on the window to mount it (slot guessed from size) |

**Emulator hotkeys** — inherited from KEGS and handled by the emulator core:

| Key | Action |
|---|---|
| **F4** | Open the configuration menu (mount disks, pick a ROM, settings) |
| **F5** | Toggle the status line |
| **F6** | Cycle emulation speed (1 MHz / 2.8 MHz / fast) |
| **F7** | Toggle the debugger · **Shift + F7** toggles fast disk emulation |
| **F8** | Grab / release the mouse (warp + hide pointer) |
| **F9** | Invert paddles · **Shift + F9** swap paddles · **Ctrl + F9** copy screen text |
| **Ctrl + F12** | Reset the IIgs (Ctrl-Reset) |

On keyboards without Apple keys, **F1** acts as Open-Apple (⌘), **F2** as
Option/Closed-Apple, and **F3** as Escape. On macOS, ⌘ key combos are forwarded
to the emulated machine.

## Building from source

GSplus builds with CMake (Ninja generator). SDL3 is used if installed, otherwise
built from source automatically. From the repository root:

```sh
cmake -G Ninja -B build -S gsplus/src
cmake --build build --target gsplus-sdl
```

This produces the GSplus app:

- **macOS:** `build/GSplus.app`
- **Linux:** `build/gsplus`
- **Windows:** `build/gsplus.exe` (with `SDL3.dll` alongside)

### Dependencies

**macOS** (Homebrew, plus the Xcode Command Line Tools):

```sh
brew install cmake ninja sdl3
```

**Linux** (Debian/Ubuntu) - the X11/Wayland/audio packages are SDL3's build
dependencies:

```sh
sudo apt install cmake ninja-build build-essential git \
  libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev \
  libxfixes-dev libxss-dev libxtst-dev libxinerama-dev \
  libwayland-dev wayland-protocols libdecor-0-dev libxkbcommon-dev \
  libdrm-dev libgbm-dev libegl1-mesa-dev libgl1-mesa-dev libgles2-mesa-dev \
  libpulse-dev libasound2-dev libpipewire-0.3-dev libsndio-dev \
  libdbus-1-dev libudev-dev libibus-1.0-dev
```

**Windows** ([MSYS2](https://www.msys2.org/), in the MINGW64 shell):

```sh
pacman -S git mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja
```

## About Branches

- KEGS latest is tracked in the `./upstream` directory.
- An `upstream` branch is updated whenever there's a new KEGS version and merged
  to `main`.
- This makes it easy to track KEGS changes somewhat independently of the GSplus
  work.

## A note from Dagen

I've tried to reboot GSplus _many times_ over the past few years and each time 
I'd sink weeks into building and packaging a new release, and then life would 
pull me away for months, sometimes years, and momentum would die. I made the 
decision to use AI to allow me to provide the emulator I wanted to have, which
is controversial even for me.  But then again, this isn't any different than 
what I see in every modern development shop.  Things have changed dramatically
over the past few years.  But I want to be clear about AI usage, and that I
feel comfortable using it as a tool that needs a lot of oversight. 

KEGS is a brilliant piece of pure C, and is living proof that you don't need
AI to write great software. Kent Dickey did the hard, beautiful work. The AI 
here is just a pragmatic crutch to keep *my* reboot alive between the demands 
of real life.

So what does GSplus actually add? A handful of **features I want for myself**.
If you want the canonical emulator, go straight to KEGS.  It's great!

- Dagen
