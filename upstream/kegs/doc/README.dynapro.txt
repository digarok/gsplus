
DYNAPRO 
-------

Dynapro is a KEGS feature where an emulated disk (140KB 5.25" in slot 6, 800KB
3.5" in slot 5, or any size up to 32MB in slot 7) volume is mapped to a host
(Mac, Linux) directory (and subdirectories).  This allows easy access to move
files from emulation--just look at the files from outside KEGS on your host
computer, they are always up to date.  It also provide a simple way to move
files into emulation: update the file under the Dynapro mount point, and then
remount the volume to ensure KEGS/ProDOS can see the changes.

How to enable
-------------

Create a folder on your Mac/Linux system.  You can place files in it, or leave
it empty.  From within KEGS, press F4, select Disk Configuration, and then use
arrows to move to the slot and drive you want to mount that folder as.

You have two choices: A slightly longer process where you can set the DynaPro
image size (if you want less than 32MB for a slot 7 image), or a shorter
process that always mounts the largest size for that slot.

   Selecting the size:
   ------------------

Press "n" to create a new image.  Select the size you would like with
left/right arrows if you picked slot 7.  (slot 5 is always 800KB, and slot 6
is always 140KB).  It's fine to always pick 32MB, the default.  Scroll down to
Type, select "Dynamic ProDOS directory".  Scroll down to "Create and name the
image".  Use the normal image selection interface to navigate to the directory
you want to use.  Press tab to switch to the path view (you can manually enter
parts of the path if you like), and while on the path, press Cmd-Enter to
select the named directory.  Note: The directory that will be selected is the
path listed on the "Path: " line (relative to the Current KEGS Directory if it
doesn't start with a "/").

   Using the maximum size:
   ----------------------

Press return on the slot/drive line you want to mount as a Dynapro directory.
Using selection, select into the directory you want to use, then press
Cmd-Enter to select that directory.  A maximum size Dynapro directory will be
mounted.  The maximum size for slot 7 images is 32767.5KB (65535 blocks).

How to use
----------

If you pick a directory that has files whose total size exceeds the size you
selected, it will fail to mount.  The slot/drive entry will be commented out
showing no disk mounted.

Now, from within KEGS, you can do any operation to this volume that you would
have if it were a normal disk image--copy files, initialize it, etc.  This
works under ProDOS 8 or GS/OS, or any program that supports ProDOS images.
All changes to the image as ProDOS volume are immediately reflected in the
directory on your host machine.

Gotchas to watch out for
------------------------

KEGS aggressively makes the directory match the ProDOS volume.  If you erase a
file using GS/OS (as an example), KEGS will erase that file from the directory
you selected.  If you format the volume, KEGS will erase all files in the
directory you selected.  So: be careful what you point KEGS at!

How it works
------------

KEGS keeps a version of the ProDOS volume in memory (so 32MB if you mount a
32MB Dynapro directory).  Whenever any writes happen, KEGS looks to see what
file(s) are affected, and reparses them to recreate the files in the Dynapro
directory on your host system.  Since ProDOS and GS/OS write one block at a
time, there are many times during a write to a file where the file is invalid.
KEGS erases all files from the Dynapro host directory when the ProDOS file
appears invalid, even briefly.  So, writing to a file can cause it to be
erased from the Dynapro host directory until the file is completely written
and appears valid again.

How to copy files from your host system to KEGS emulation
---------------------------------------------------------

ProDOS and GS/OS do not expect ProDOS volumes to change behind their back.  To
reliably transfer files from your host system into the emulated //gs system,
you should unmount and re-mount the Dynapro directory.  This tells ProDOS 8
and GS/OS to expect changes to the volume, and to see any changes you've made.

So if you create a new file called "NEW.TXT" in the Dynapro host directory, it
doesn't show up in KEGS right away.

Press F4, go to the Disk Configuration page (you may already be there if you
left the F4 configuration here), select the slot/drive of the Dynapro volume
to remount and press Return.  A file selection dialog appears.  Just press
Cmd-Enter to re-mount the image (the selection for the existing volume will
already be correct).  Press F4 to return to the emulated system.

