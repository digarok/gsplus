
Place a ROM, ROM.01 or ROM.03 file in your home directory, or in the
directory you run KEGS from, or in the directory you placed a config.kegs
file.  You may manually select the ROM to use while running KEGS by
pressing F4, and then selecting "ROM File Selection", then "ROM File", and
then using the file selector to locate the ROM file.

KEGS supports Apple //e ROMs as well.  The ROM needs to be 32KB, with the
actual 16KB of useful ROM in the last 16KB, and requires a Disk II PROM
at offset $600 in the file (this just seems to be the format other emulators
use, and I'm just following it).

There are several places that have Apple IIgs ROMs.  Most work, here's a
list of ROM files which will work with KEGS:

KEGS accepts the following ROM file names (if you have a file with this name
at the right location, it will just work):

ROM, ROM.01, ROM.03, APPLE2GS.ROM, APPLE2GS.ROM2, xgs.rom, XGS.ROM,
Rom03gd, 342-0077-b.

rom01.zip contains APPLE2GS.ROM.  This file is a good ROM01
	a rom with this name and use it.
rom03.zip contains APPLE2GS.ROM2.  This file is a good ROM03
xgs.rom is a good ROM01.
iigs_rom01.zip contains APPLE2GS.ROM.  This file is a good ROM01.
iigs_rom03.zip contains APPLE2GS.ROM2.  This file is a good ROM03.
gsrom01.zip contains xgs.rom.  This file is a good ROM01.
gsrom03.zip contains Rom03gd.  This file is a good ROM03.
appleiigs_rom01.zip contains XGS.ROM.  This file is a good ROM01.
apple2_roms.zip contains xgs.rom.  This file is a good ROM01.
	It also contains gsrom03.zip which contains Rom03gd which is a
	good ROM03.
	It also contains gsrom01.zip which contains xgs.rom again, which is
	a good ROM01.


Files that won't work:
APPLE Computer and Peripheral Card Roms Collection.zip: Don't use, the
	file "Apple IIgs ROM1 - 342-0077-B -  27C1001.bin" is not in the
	format KEGS expects.

MAME ROM files can often be found with the search
"site:archive.org mame-merged", with the GS ROM in apple2gs.zip.
The file 342-0077-b is a ROM.01 file and KEGS will just use it (if you place
it in the proper location).  The 341-0728 and 341-0748 files can be a
good ROM03 file, but it's in 2 files (KEGS expects ROMs in a single file),
and the second file is in a weird order (MAME expects contributors to
remove ROMs from systems, use a ROM reader to dump out the raw contents,
and then it uses that, and apparently this puts the data in a weird order
for ROM.03 images).  You can convert these to files to the ROM.03 KEGS
expects as follows on Mac or Linux:

dd if=341-0748 of=tmp1 bs=64k skip=1
dd if=341-0748 of=tmp2 bs=64k count=1
cat 341-0728 tmp1 tmp2 > ROM.03

KEGS does not support ROM0 ROMs.

--------------------------------------------------------------------------

How to capture the ROM file from an Apple IIgs
----------------------------------------------

Enter this BASIC program after booting PRODOS 8, and have available at least
128KB (or 256KB for ROM.03) on the disk:

10 D$ = CHR$ (4)
20  GOSUB 200:P = 0
30  FOR I = 254 TO 255
40  POKE 778,I
50  FOR J = 0 TO 192 STEP 64
60  POKE 777,J
70  CALL 768
80  PRINT D$;"BSAVE ROM,A$2000,L$4000,B";P
90  PRINT P:P = P + 16384
100  NEXT : NEXT
150  END
200  FOR I = 0 TO 20
210  READ D: POKE 768 + I,D
220  NEXT : RETURN
230  DATA 24,251,194,48,162,254,63,191
240  DATA 0,0,254,157,0,32,202,202
250  DATA 16,245,56,251,96

RUN

When typing it in, the spaces are all optional you don't need to type them.

This program captures the 128KB of a ROM.01 into the file "ROM".
To capture the 256KB of a ROM.03, change line 30 to:

30  FOR I = 252 TO 255

----------------------------------------------------------------------------

