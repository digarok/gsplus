
MAC OS X port of KEGS (KEGSMAC): http://kegs.sourceforge.net
-------------------------------------------------------------

This is a Mac OS X Swift port.  Most of KEGS is written in C, but the
UI is in Swift.

Downloading, and making it runnable:
-----------------------------------

Download from http://kegs.sourceforge.net/.  If you're using Safari in it's
default mode (unpack "Safe" files after downloading), you'll end up with
kegs.1.xx.tar in your Downloads/ directory.  (where xx will be the version).
Mac OS X will not let you run unsigned quarantined code you find on the net
(which is good, generally), so you need to remove quarantine.  At a Terminal
window, cd to where you want to install KEGS (it can just be your home
directory):

cd
cat ~/Downloads/kegs.1.xx.tar | tar xvf -

If you have a compressed file like kegs.1.xx.tar.gz, do:

cd
gunzip < ~/Downloads/kegs.1.xx.tar.gz | tar xvf -

You will now have a directory names kegs.1.xx and in that, KEGSMACK.app/.

If you've let Safari or the Finder unpack KEGS, then you can do this command
from the Terminal in the directory you unpacked KEGSMAC to remove the
quarantine:

xattr -r -d com.apple.quarantine KEGSMAC.app

Usage:
-----

Like most other KEGS versions, KEGSMAC is best run from a Terminal
window.  Just type "./KEGSMAC.app/Contents/MacOS/KEGSMAC" in the directory
you installed/compiled it in.  You need to have a ROM file (named
ROM, ROM.01, or ROM.03) and a config.kegs in the same directory or in your
home directory (read the README--these files are searched for in various
places).

KEGSMAC can also be run from the Finder by double-clicking on the icon, or
by doing "open KEGSMAC", but if you do this, debugging problems is a little
tougher.

To quit, either click the window close box, or select Quit from the menu.
To go full screen, click the Maximize button in the window.  You can
resize the window to any size you like.

Mac OS X now has "quarantine" and Disk Access Permissions, which can be a
pain for users of KEGSMAC.  You must remove Quarantine or it won't work at
all, but the disk access permissions are used to protect special Folders like
Documents and Downloads.  System Preferences->Security&Privacy->Files and
Folders should let you add permissions to KEGS--but it doesn't seem to
always stick.  I'm not sure why this is.  A simple workaround is to
place your ROMs and disk images in a directory like "Apple2" or something
similar in your home directory.  Then KEGSMAC won't need access to Desktop
or Downloads and should work better.  If you have code development
suggestions on how to have KEGSMAC work better, please let me know.

Compile directions
------------------

See README.compile.txt

