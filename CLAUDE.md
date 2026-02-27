# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

GSplus is a cross-platform Apple IIgs emulator based on KEGS (Kent's Emulated GS) by Kent Dickey. It emulates the 65816 CPU, all Apple IIgs graphics/sound modes, disk controllers, serial ports, and more. Licensed under GPLv3.

## Repository Structure

- `gsplus/src/` — Active source code and build files (this is where you build)
- `gsplus/lib/` — Icons, NIB files, and asset resources
- `upstream/kegs/` — Upstream KEGS tracked separately, merged to main when updated
- `upstream/kegs/doc/` — Comprehensive documentation (architecture internals, platform setup, compatibility)

The `upstream` branch tracks KEGS releases; `main` is the primary development branch.

## Build Commands

### macOS (default target)
```bash
cd gsplus/src
make -j 20
```
Produces `gsplus/KEGSMAC.app`. Requires Xcode with command-line tools installed.

### Linux (X11)
```bash
cd gsplus/src
rm vars; ln -s vars_x86linux vars
make -j 20
```
Produces `xkegs`. Requires `libX11-devel`, `libXext-devel`, `pulseaudio-libs-devel`.

### Windows
Open `gsplus/src/kegswin.vcxproj` in Visual Studio Community Edition and press F7.

### Clean
```bash
cd gsplus/src
make clean
```

## Architecture

### CPU & Core Loop
- `sim65816.c` — Main simulation loop, event scheduling, interrupt handling
- `engine_c.c` + `engine.h` — 65816 CPU instruction emulation (macro-heavy for performance)
- `defs_instr.h`, `instable.h`, `op_routs.h` — Instruction definitions, opcode table, operation macros

### Memory
- `moremem.c` — Memory management, page table fixup, I/O register reads/writes ($C000-$C0FF area)
- Page-table-based MMU for dynamic address mapping

### Video
- `video.c` — All Apple IIgs/II graphics mode rendering (text, lores, hires, super hires)

### Audio
- `sound.c` — Sound generation (mixing, output buffering)
- `doc.c` — Ensoniq DOC 32-voice synthesizer emulation
- `mockingboard.c` — Mockingboard A card (6522 VIA + AY-8913)

### Disk I/O
- `iwm.c` — IWM disk controller (5.25" and 3.5" drives, nibble-level accuracy)
- `smartport.c` — SmartPort controller for hard drive images
- `dynapro.c` — Host directory mounting as virtual ProDOS volumes
- `woz.c` — WOZ disk image format support
- `unshk.c`, `undeflate.c`, `applesingle.c` — Archive/compression format support

### Input
- `adb.c` — Apple Desktop Bus (keyboard, mouse)
- `scc.c` + `scc_socket_driver.c` — Serial Communications Controller with TCP/IP modem emulation
- `paddles.c`, `joystick_driver.c` — Game input

### Configuration & Debug
- `config.c` — Runtime configuration UI, disk mounting, settings persistence (`config.kegs`)
- `debugger.c` — Built-in 65816 debugger/monitor

### Platform Drivers
The emulator core is platform-independent. Platform-specific code is isolated in driver files:

| Component | macOS | Linux | Windows |
|-----------|-------|-------|---------|
| Display | `AppDelegate.swift` + `MainView.swift` | `xdriver.c` | `windriver.c` |
| Audio | `macsnd_driver.c` (CoreAudio) | `pulseaudio_driver.c` | `win32snd_driver.c` |
| Serial | `scc_unixdriver.c` | `scc_unixdriver.c` | `scc_windriver.c` |

### Key Headers
- `defc.h` — Global defines, structs, macros (included by nearly every .c file)
- `defcomm.h` — Shared defines for C and assembly
- `protos_base.h` — Function prototypes for all modules

## Build System Details

The Makefile includes `vars` (platform config) and `ldvars` (object file list). To change platforms, symlink the appropriate vars file (`vars_mac`, `vars_x86linux`, etc.) to `vars`. Swift files are compiled via the `comp_swift` wrapper script. The `dependency` file contains header dependency rules.

Compiler flags are set in `vars`: `-Wall -O2 -DMAC` for macOS. The `-DMAC` define selects macOS-specific code paths throughout the codebase.

## Runtime Requirements

The emulator needs an Apple IIgs ROM file to run. Demo disk images (`NUCLEUS03.gz`, `XMAS_DEMO.gz`) are included in `upstream/kegs/`. Configuration is stored in `config.kegs`.

## No Test Suite

There is no automated test infrastructure. Testing is done manually by running Apple IIgs software and ROM self-tests.
