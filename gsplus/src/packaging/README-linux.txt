GSplus on Linux
===============

GSplus needs the SDL3 library installed on your system. If launching
fails with an error like:

    error while loading shared libraries: libSDL3.so.0:
    cannot open shared object file: No such file or directory

install SDL3 for your distribution:

  Debian 13 "trixie" or newer, Ubuntu 25.04 or newer:
      sudo apt install libsdl3-0

  Fedora:
      sudo dnf install SDL3

  Arch:
      sudo pacman -S sdl3

Older releases (Debian 12, Ubuntu 24.04 LTS, and earlier) do not package
SDL3 at all -- build it from source:

    # Dev packages SDL needs for video/audio support; see
    # https://wiki.libsdl.org/SDL3/README-linux for the full list.
    sudo apt install build-essential cmake git \
        libx11-dev libxext-dev libwayland-dev libxkbcommon-dev \
        libegl1-mesa-dev libpulse-dev libasound2-dev

    git clone https://github.com/libsdl-org/SDL
    cmake -S SDL -B SDL/build -DCMAKE_BUILD_TYPE=Release
    cmake --build SDL/build -j$(nproc)
    sudo cmake --install SDL/build
    sudo ldconfig

Running
-------

GSplus also needs an Apple IIgs ROM file and a config file to be useful.
See README.md (included alongside this file) for setup instructions,
hotkeys, and everything else.
