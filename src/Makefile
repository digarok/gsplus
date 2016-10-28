# GSplus central makefile - you need a 'vars' file linked/copied from a 'vars_xxx' template to build.
#@todo: check for vars file and do something useful if missing :P
#@todo: remove all packaging junk and move to packaging scripts in CI
OBJECTS1 = adb.o clock.o config.o debug.o dis.o engine_c.o scc.o iwm.o \
	joystick_driver.o moremem.o paddles.o parallel.o printer.o sim65816.o \
	smartport.o sound.o sound_driver.o video.o scc_socket_driver.o glog.o \
	imagewriter.o scc_imagewriter.o scc_llap.o
ATOBJ = atbridge/aarp.o atbridge/atbridge.o atbridge/elap.o atbridge/llap.o atbridge/port.o
PCAPOBJ = atbridge/pcap_delay.o
TFEOBJ = tfe/tfe.o tfe/tfearch.o tfe/tfesupp.o

include vars

.SUFFIXES: .dep .proto

AS = $(CC)

XLIBS = -L/usr/X11R6/lib
PERL = perl

all: $(TARGET)

clean:
	- rm -f $(OBJECTS)
	- rm -f $(TARGET)
	- rm -f compile_time.o
	- rm -f 8inst_c.h
	- rm -f 16inst_c.h

specials: 8inst_s 16inst_s 8size 16size 8inst_c 16inst_c size_c size_s

specials_clean:
	rm -f 8inst_s 16inst_s 8size 16size 8inst_c 16inst_c size_c size_s

# Linux/OSX SDL builds
gsplus: $(OBJECTS) compile_time.o
	$(LD) $(CCOPTS) $(LDOPTS) $(OBJECTS) compile_time.o $(LDFLAGS) -o $(NAME)$(SUFFIX) $(EXTRA_LIBS)
	echo $(OBJECTS)
	cp gsplus ..

# Linux/OSX XWindows builds
gsplusx: $(OBJECTS) compile_time.o
	$(LD) $(CCOPTS) $(LDOPTS) $(OBJECTS) compile_time.o $(LDFLAGS) -o $(NAME)$(SUFFIX) $(XLIBS) $(EXTRA_LIBS) -lX11
	echo $(OBJECTS)
	mv gsplusx ..

# NOT CURRENTLY SUPPORTED
# Linux framebuffer builds:
gsplusfb: $(OBJECTS) compile_time.o
	$(LD) $(CCOPTS) $(LDOPTS) $(OBJECTS) compile_time.o $(LDFLAGS) -o $(NAME)$(SUFFIX) $(EXTRA_LIBS)
	echo $(OBJECTS)
	mv gsplusfb ..

# Mingw32 / Cygwin builds:  The Win32 API version
gsplus32.exe: $(OBJECTS) compile_time.o
	g++ $(CCOPTS) $(LDOPTS) $(OBJECTS) compile_time.o $(LDFLAGS) -o $(NAME)$(SUFFIX) $(EXTRA_LIBS) -lwinmm -lgdi32 -ldsound -lcomctl32 -lws2_32 -lshell32
	#mkdir -p ../GSplus.app/lib
	#cp -f gsplus.exe ../GSplus.app/GSplus.exe
	#cp -f ../config.template ../GSplus.app/config.txt
	#cp -f ../lib/*.ttf ../GSplus.app/lib
	#cp -f ../lib/arch/win32/*.dll ../GSplus.app
	#cp -f ../lib/NoBoot.po ../GSplus.app
	#cp -f GSplus.bat ../GSplus.app/GSplus.bat
	#cp -f parallel.rom ../GSplus.app
	#cp -f ../COPYING.txt ../GSplus.app
	cp gsplus32.exe ..

# Mingw32 / Cygwin builds:  The SDL version (builds, but non-functioning)
gsplus.exe: $(OBJECTS) compile_time.o
	#g++ $(CCOPTS) $(LDOPTS) $(OBJECTS) compile_time.o $(LDFLAGS) -o $(NAME)$(SUFFIX) $(EXTRA_LIBS) -lwinmm -lgdi32 -ldsound -lcomctl32 -lws2_32 -lshell32
	g++ $(CCOPTS) $(LDOPTS) $(OBJECTS) compile_time.o $(LDFLAGS) -o $(NAME)$(SUFFIX) $(EXTRA_LIBS)  -lSDLmain -mwindows
	#mkdir -p ../GSplus.app/lib
	#cp -f gsplus.exe ../GSplus.app/GSplus.exe
	#cp -f ../config.template ../GSplus.app/config.txt
	#cp -f ../lib/*.ttf ../GSplus.app/lib
	#cp -f ../lib/arch/win32/*.dll ../GSplus.app
	#cp -f ../lib/NoBoot.po ../GSplus.app
	#cp -f GSplus.bat ../GSplus.app/GSplus.bat
	#cp -f parallel.rom ../GSplus.app
	#cp -f ../COPYING.txt ../GSplus.app
	cp gsplus.exe ..

# NOT CURRENTLY SUPPORTED
# Mac build - old style (deprecated)
gsportmac: $(OBJECTS) compile_time.o
	$(CC) $(CCOPTS) $(LDOPTS) $(OBJECTS) compile_time.o $(LDFLAGS) -o gsport $(EXTRA_LIBS)
	mkdir -p ../GSport.app/Contents/Resources/English.lproj/main.nib
	mkdir -p ../GSport.app/Contents/MacOS
	mv gsport ../GSport.app/Contents/MacOS/GSport
	echo "APPL????" > ../GSport.app/Contents/PkgInfo
	cp -f arch/mac/Info.plist ../GSport.app/Contents/
	cp -f arch/mac/info.nib ../GSport.app/Contents/Resources/English.lproj/main.nib
	cp -f arch/mac/classes.nib ../GSport.app/Contents/Resources/English.lproj/main.nib
	cp -f arch/mac/objects.xib ../GSport.app/Contents/Resources/English.lproj/main.nib
	cp -f arch/mac/gsporticon.icns ../GSport.app/Contents/Resources/
	cp -f arch/mac/525.icns ../GSport.app/Contents/Resources/
	cp -f arch/mac/2mg.icns ../GSport.app/Contents/Resources/
	touch '../GSport.app/Icon?'
	rm -rf ../GSportDmg
	mkdir ../GSportDmg
	mkdir ../GSportDmg/GSport
	cp ../COPYING.txt ../GSportDmg/GSport
	cp -f parallel.rom ../GSportDmg/GSport
	cp -f ../lib/NoBoot.po ../GSportDmg/GSport
	mv ../GSport.app ../GSportDmg/GSport
	cp -f ../config.template ../GSportDmg/GSport/config.txt
	cp ../GSport.html ../GSportDmg/GSport/GSport.html
	arch/mac/makedmg.sh .. GSportDmg GSport GSport 7

8inst_c.h: instable.h
	$(PERL) make_inst c 8 instable.h > 8inst_c.h

16inst_c.h: instable.h
	$(PERL) make_inst c 16 instable.h > 16inst_c.h

size_c.h: size_tab.h
	$(PERL) make_size c size_tab.h > size_c.h

engine_c.o: 8inst_c.h 16inst_c.h size_c.h

8inst_s.h: instable.h
	$(PERL) make_inst s 8 instable.h > 8inst_s.h

16inst_s.h: instable.h
	$(PERL) make_inst s 16 instable.h > 16inst_s.h

size_s.h: size_tab.h
	$(PERL) make_size s size_tab.h > size_s.h

8size_s.h: size_tab.h
	$(PERL) make_size 8 size_tab.h > 8size_s.h

16size_s.h: size_tab.h
	$(PERL) make_size 16 size_tab.h > 16size_s.h

engine_s.o: 8inst_s.h 16inst_s.h 8size_s.h 16size_s.h size_s.h

.s.o:
	$(AS) -c $(OPTS) -I. $*.s

.c.o:
	$(CC) $(CCOPTS) $(XOPTS) -c $(OPTS) -I. -o $*.o $*.c

.cpp.o:
	$(CC) $(CPPOPTS) $(XOPTS) -c $(OPTS) -I. $*.cpp

.cpp.O:
	$(CC) $(CCOPTS) $(XOPTS) -c $(OPTS) -I. $*.c

partls: partls.c
	cc $(CCOPTS) $(XOPTS) $(OPTS) -o partls partls.c

to_pro: prodos.h prodos_protos.h to_pro.c
	cc $(CCOPTS) $(XOPTS) $(OPTS) -o to_pro to_pro.c

gsplus32.o:   win32.rc winresource.h
	windres win32.rc -o gsplus32.o

compile_time.o: $(OBJECTS)


# dependency stuff
adb.o: adb.c adb.h defc.h defcomm.h iwm.h protos.h
atbridge/aarp.o: defc.h atbridge/atbridge.h atbridge/port.h atbridge/elap.h atbridge/aarp.h atbridge/elap_defs.h
atbridge/atbridge.o: defc.h atbridge/atbridge.h atbridge/port.h atbridge/elap.h atbridge/llap.h atbridge/aarp.h
atbridge/elap.o: defc.h atbridge/atbridge.h atbridge/port.h atbridge/elap.h atbridge/aarp.h atbridge/elap_defs.h atbridge/pcap_delay.h
atbridge/llap.o: defc.h atbridge/atbridge.h atbridge/port.h atbridge/llap.h
atbridge/port.o: atbridge/atalk.h atbridge/port.h
atbridge/pcap_delay.o: atbridge/pcap_delay.h
engine_c.o: engine_c.c defc.h defcomm.h iwm.h protos.h protos_engine_c.h size_c.h op_routs.h defs_instr.h 8inst_c.h 16inst_c.h
clock.o: clock.c defc.h defcomm.h iwm.h protos.h
compile_time.o: compile_time.c
config.o: config.c defc.h defcomm.h iwm.h protos.h config.h
dis.o: dis.c defc.h defcomm.h iwm.h protos.h disas.h
scc.o: scc.c defc.h defcomm.h iwm.h protos.h scc.h
scc_llap.o: atbridge/atbridge.h atbridge/llap.h defc.h scc.h
scc_socket_driver.o: scc_socket_driver.c defc.h defcomm.h iwm.h protos.h scc.h
scc_windriver.o: scc_windriver.c defc.h defcomm.h iwm.h protos.h scc.h
scc_macdriver.o: scc_macdriver.c defc.h defcomm.h iwm.h protos.h scc.h
scc_imagewriter.o: scc_imagewriter.c defc.h defcomm.h protos.h scc.h
iwm.o: iwm.c defc.h defcomm.h iwm.h protos.h iwm_35_525.h
imagewriter.o: imagewriter.cpp
joystick_driver.o: joystick_driver.c defc.h defcomm.h iwm.h protos.h
moremem.o: moremem.c defc.h defcomm.h iwm.h protos.h
paddles.o: paddles.c defc.h defcomm.h iwm.h protos.h
parallel.o: parallel.c defc.h
printer.o: printer.cpp
sim65816.o: sim65816.c defc.h defcomm.h iwm.h protos.h
smartport.o: smartport.c defc.h defcomm.h iwm.h protos.h
sound.o: sound.c defc.h defcomm.h iwm.h protos.h sound.h
sound_driver.o: sound_driver.c defc.h defcomm.h iwm.h protos.h sound.h
video.o: video.c defc.h defcomm.h iwm.h protos.h superhires.h gsportfont.h
tfe.o: tfe/tfe.c tfe/tfe.h tfe/tfe_protos.h
tfearch.o: tfe/tfearch.c tfe/tfearch.h tfe/tfe_protos.h
tfesupp.o: tfe/tfesupp.c tfe/tfesupp.h tfe/tfe_protos.h
macdriver.o: macdriver.c defc.h defcomm.h iwm.h protos.h protos_macdriver.h
macdriver_console.o: macdriver_console.c defc.h defcomm.h iwm.h protos.h protos_macdriver.h
macdriver_generic.o: macdriver_generic.c defc.h defcomm.h iwm.h protos.h protos_macdriver.h
macsnd_driver.o: macsnd_driver.c defc.h defcomm.h iwm.h protos.h sound.h
windriver.o: windriver.c defc.h defcomm.h iwm.h protos.h protos_windriver.h winresource.h gsplus32.o
win_console.o: win_console.c defc.h defcomm.h iwm.h protos.h protos_windriver.h winresource.h
win_generic.o: win_generic.c defc.h defcomm.h iwm.h protos.h protos_windriver.h winresource.h
win32snd_driver.o: win32snd_driver.c defc.h defcomm.h iwm.h protos.h sound.h
