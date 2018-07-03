# kegs-universal

Minimal changes on the kegs apple2-gs emulator to accept and run  apple 2, 2+, 2e, 2c, and 2c+ roms.


I was curious to see how difficult it would be to boot the KEGS Apple 2gs emulator 
using the ROMs of older Apple 2 models. Since the 2gs hardware was designed to be
as compatible as possible with the earlier models, it likely supports what the 
older ROMs need. This reasoning worked quite well for the Apple 2, 2+, 
and 2e ROMs. The changes needed to support the Apple 2c and 2c+ ROMs were 
much more interesting. This is described below.

## Supported ROMs

The KEGS configuration code (`config.c`) was minimally changed to accept ROM
files of 12KB (for the Apple 2 and 2+), 16KB (for the Apple 2e and some 2c),
32KB (for the Apple 2c and 2c+). It also uses the standard tests to
determine the type of the machine associated with the ROM and
set the variable `g_a2rom_version` with the following values:
  * `'2'` for an Apple 2 or Apple 2+ ROM (both 12KB roms,)
  * `'e'` for an Apple 2e (both the 16KB original and enhanced roms,)
  * `'c'` for an Apple 2c  (tested the 16KB rom0 and the 32KB rom4,)
  * `'C'` for an Apple 2c+ (the 32KB rom5,)
  * `'g'` for an Apple 2gs (both the 128KB and 256KB roms.)

I also added a configuration option to load the disk controller ROM
into the slot 6 rom space. This is useful with the 2, 2+, and 2e ROMs.
The 2c, 2c+, and 2gs ROMs provide this code.

## Apple 2 and 2+

The Apple 2 and 2+ ROMs booted right away.
For convenience I added code in `moremem.c` to map all alpha keys to uppercase
and to map the delete key to backspace. I also noticed and fixed subtle but
important differences over time:
  * The 2gs resets the banked memory in a known state at every reset. 
    The original language card settings were preserved. The reset code
    in file `sim65816.c` now selectively initializes registers
    depending on the detected rom type.
  * The 2gs contains a special circuit that causes the processor to
    always fetch interruption vectors in the rom located in page `ff` 
    instead of page `00`. The corresponding code, located in `sim65816.c`,
    now only does this if `g_a2rom_version=='g'`.
  * The KEGS emulator did not implement the language card memory protection
    and the double reading of the `c08x`. This was fixed in `moremem.c`
    for all architectures. A couple weeks later I also had to modify the
    cpu simulation code for INC abs,X because several pieces of code depend
    on the 6502/65C02/65816 reading the memory location twice to write-enable
    the language card ram.
    
Note that the character generator ROM still shows characters `e0..ff`
as lowercase character. On the real Apple 2 or 2+, these are uppercase
characters.

## Apple 2e

The Apple 2e ROMs also booted right away and even passed the self test.
This is not surprising as the 2gs is designed to be maximally 
compatible with the 2e.

## Apple 2c

Getting the Apple 2c to work represented a lot more work.
It turns out that the 2e and the 2c often follow different paths
to remain compatible with the original Apple 2. For instance,
in the 2e or the 2gs, reading `c019` merely tells you whether
we are in the video vertical blanking time window. On the 2c,
this same register tells you whether there is a pending vertical
blanking interrupt and clears the interrupt. All these details
are fixed in `moremem.c`.

### Rombank

The later Apple 2c ROMS contain 32KB of code. The config file
loads the two 16KB segments in the 2gs pages `ff` and `fe`.
Which ROM bank is shown in page `00` then depends
on the RDROM, CXINT, and ROMBANK bits of the `c068` state
register. The KEGS emulator did not support the ROMBANK
bit and I implemented it to use the rom from page `fe` 
instead of `ff`. The Apple 2c port `c028` now
switches the active bank by calling the normal
function `set_statereg()` defined in `moremem.c`.

### Slinky

The later Apple 2c also support a memory expansion card
that can be used as a virtual drive. It turns out that
some versions of the 2c ROMs do not boot properly when
the corresponding i/o ports `c0cx` are not implemented.
Simple code in `moremem.c` now uses up to 1MB of RAM
in pages `02` to `12` to implement the memory extension.

### The Apple 2c mouse

Then I decided to resolve the dramatically different ways 
in which the 2c and the 2gs handle the mouse. Whereas the 2gs 
uses the rather intelligent adb controller to buffer the mouse 
moves and report x or y deltas in range 0..64, the 2c interrupts 
the 65C02 for every little move. Each access to an Apple 2c
mouse register now calls a routine that pumps the mouse state
from the 2gs adb controller (function `emulate_a2c_mouse()` in 
file `moremem.c`). In parallel, two small changes in `adb.c` were 
necessary in order to send mouse deltas in smaller increments
and to prevent the 2gs from interrupting the cpu
when the mouse button is pressed without mouse move.
Prodos really dislikes unhandled interrupts.

### Serial ports

I did not try to emulate the ACIA or map it to the 2gs SCC.

### 3.5" drives.

Later version of the Apple 2c also supports 3.5" drives.
However these are not the same as the 2gs 3.5" drives.
The 2c only uses the Unidisk 3.5" that contain
their own 6502 variant clocked at 2MHz, that is
twice faster than the 2c 65C02 cpu.  The proper way
to implement this would be to have the emulator
intercept firmware calls on slot 5. This technique
is already used by Kegs to simulate a hard disk
on slot 7. On the other hand, the 2c+ uses the
same drives as the 2gs...

## Apple 2c+

Surprisingly the changes that made the late Apple 2c ROMs 
work smoothly allowed KEGS to boot the Apple 2c+ ROM and
even pass the self test. 

Then I came with the crazy idea to make the 2gs 3.5" drives
work with the Apple 2c+ ROM. This can work because the
Apple 2c+ uses the same dumb 3.5" drives as the 2gs.
The difficult is that the Apple 2c+ does so using
the special and poorly documented MIG chip and
using a small 2K cache RAM.  

The MIG is rumored to be necessary to allow the 1MHz 65C02 to 
keep up with the 3.5" drive data transfer rate. The rumor
says that the Apple engineers only kept the MIG on the 2c+ board
because the 4MHz built-in cpu accelerator was a late addition
to the project. The only programming information I found
on the MIG was some 
[reverse engineering by "M.G."](http://apple2.guidero.us/doku.php?id=mg_notes:apple_iic:mig_chip)
and the discussion following a 
[bug report for the MAME emulator.](https://github.com/mamedev/mame/issues/2775)

Two realization helped me figure out the rest. First, in the available
2c schematics, the MIG RESET line is tied to both the /IOSEL line 
and the A14 ROM line which selects the alternate 16KB ROM bank.
In other words, the MIG can only be accessed when the alternate
ROM bank is selected. Whenever the firmware returns to the main ROM bank,
the MIG is reset to its default, 5.25" inch drive-compatible, state.
This is probably why M.G. only saw brief pulses on the drive lines
when triggering the MIG i/o ports.  The second realization was
that the Apple 2c+ supports three dumb 3.5" drives: the internal drive
and two external daisy chained drives. The MIG contains a switch
that either directs the IWM DR2 enable to the internal disk
or to the external port. This makes the internal disk a d2
while the external disks are d1 and d2. After figuring out
how the MIG i/o ports control this switch as well as the 
3.5SEL and HDSEL lines, I had the tricky problem to remap
the internal d2 and the external d1 onto the two external d1 and
d2 drives implemented by KEGS. This is all implemented
in file `moremem.c`.
    
## Apple 2gs

The behavior of KEGS with an Apple 2gs ROM is 
mostly unchanged with the following three exceptions:
  * The memory protection features of the banked language card memory
    is now implemented.
  * The slot 7 smartport hard disk emulation is now 
    switched like the ROM of a virtual card inserted in slot 7.
  * If the slot 7 code fails to find a bootable disk, the search
    for a bootable disk continues with slots 6 and 5.
   
    
  
