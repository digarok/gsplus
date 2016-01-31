GSPLUS ALPHA
============

This is an early release of an experimental project to modernize the
KEGS/GSport emulator platform and eventually extend it.

The first steps are represented here.  This release features a new SDL2 driver.
SDL or "Simple DirectMedia Layer" allows me to write one driver to handle input
and output (video, mouse, keyboard, audio, joystick).  The hope is that I can
leverage the multi-platform nature of SDL to provide first-class support for
the major plaforms supported by SDL, which includes Mac OSX, Windows and Linux.

I'm not a huge fan of OSX, but I keep a MacBook Pro nearby for work, so the
first version to be release is this OSX build.  I'll try to have builds for
other platforms available in a few weeks.



Some notes:
You need to provide the ROM file and disk images and a config file.  I'd
recommend you just drop it in your current GSport or KEGS folder and run it
from there.

I took out the "middle-click to enter debug" because it's stupid.  No one wants
to use their mouse wheel to enter a debugger.  *smacks forehead*
If you want to enter debug, it's shift-F6.  F6 is speed up, same as right and
middle mouse.

The big feature is FULLSCREEN SUPPORT! Yaya!  Hit F11 for full-screen.  My Mac
has some stupid expose feature mapped over this, so I basically hit
control-fn-F11 and it works.  Stupid Macs.  :P

I haven't changed much else at this point.  This is an experimental driver and
a very early release, so I'm not looking for feedback.  Please don't tell me
about it rendering weird, making funny noises, crashing your computer, stealing
your car, or blowing up your house.

Please be aware that this is based on KEGS original source by Kent Dickey, so
I must provide the source as per the original license.

I'd recommend you not really try building it at this time, but if you want, go
for it.

https://github.com/digarok/gsplus

I'll keep a page for this at http://apple2.gs/plus

Dagen Brock
Saturday, January 30, 2016   1:18PM
--------------------------------------------------------------------------------

Mac OSX Build Instructions
==========================
Download the repo
Install 'brew' if you don't already have it installed.  (Google it, it's easy.)
Install SDL libraries if needed:
  brew install sdl2
Install FreeType if needed:
  brew install freetype
Create a symbolic link from the vars_osx file to 'vars':
  cd src
  ln -s vars_osx vars
Build the binary:
  make clean ; make
