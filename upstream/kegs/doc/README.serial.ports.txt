
KEGS documentation: Serial Ports

SCC (Serial Port) emulation:
---------------------------

KEGS emulates the two serial ports on a IIgs as being sockets (outgoing or
incoming), or connected to real devices, or being a Virtual Modem.  The
incoming ports KEGS will use is 6501 for slot 1 (the printer port) and
6502 for slot 2 (the usual modem port).  By default, slot 1 is emulated as
a real device (but defaults to desiabled), and slot 2 emulates a Virtual
Modem.

You configure how you want the serial emulation to work on the Configuration
screen (F4), then select "Serial Configuration".  There are settings for
slot 1 and slot 2 separately.  "Main Setting" sets the overall connection
type: "Use Real Device", "Use a virtual modem", "Use Remote IP", and "Use
incoming port 6501" (or 6502).

The "Status" line under Serial Port Configuration says whether the port is
opened.  Status doesn't change while you're in the F4 page, so if you
make a change and want to see if it worked, press F4 (to leave the
Configuration screen) and F4 again (to come right back) and see what the
status is.

Apple II communications programs often want to initialize modems themselves,
and so using them with any direct connection can cause them to fail to
initialize themselves properly.  For instance, if you use ProTerm, it will
often say "failed to initialize hardware"--just keep selecting "ok", and then
force it online with Cmd-Ctrl-T.  Once "online", I always do "ATZ" to get
the text modem responses (OK, RING, etc.).

  Real Device
  -----------

On Windows, you can select COM1 through COM4 as the serial device.  You
select which one on the "Windows Device" line, and then KEGS will try to
open it.  On a Mac or Linux, KEGS let's you select any /dev/ device to be
your serial connection (if it's a USB to serial port, pretty likely since
serial ports don't exist much any more, it'll be something like
/dev/tty.usbserial-1480 on a Mac, or /dev/USB0 on Linux).  Select this on
the "Real Device" line, and it's a file selector like for disk images.
Make sure you check "Status" to make sure the device opens.


  Virtual Modem
  -------------

A Virtual Modem means KEGS acts as if a modem is on the serial port
allowing Apple II communcation programs to fully work, but connect to
internet-enabled sockets.  KEGS emulates a "Hayes-Compatible" modem,
meaning it accepts "AT" commands.  You can use KEGS to connect to free
telnet-BBSs, or run a BBS program on KEGS and become a telnet BBS yourself.
All "AT" commands in KEGS work in upper or lower case.  This description
gives all commands in upper case since it's a little easier to read.

The two main AT commands are: ATDT for dialing out, and ATA for receiving
calls.  To dial out, enter "ATDThostname", or for example,
"ATDTboycot.no-ip.com" (which is down at the moment, unfortunately).
You can also enter an IP address, like "ATDT127.0.0.1".  On a Mac or Linux,
to create a simple socket server to allow connections for testing, do in
a terminal window:

	nc -l 127.0.0.1 9111

where nc is the NetCat program (generally available), 127.0.0.1 is the IP
address of the local machine always, and 9111 is the port number (any
random number you want above 1024).  Then in your terminal program running
inside KEGS, do:

	ATDT127.0.0.1:9111

And then you can type back and forth.  "nc" buffers all input from the
keyboard, and sends it once you hit return.  And nc isn't going to do proper
terminal emulation, so return doesn't scroll to the next line.

Modems use "+++" to get the attention of the modem: press +++ within 1
second, and then wait 1 second.  KEGS will say "OK", and then you can hang
up with "ATH".

KEGS' virtual modem also accepts incoming "calls".  While using the virtual
modem on slot 2, port 6502 is open ready for incoming "calls" (unless you're
connected using ATDT to some other address).  So in your terminal program,
turn on verbose settings with "ATZ" and then connect to port 6502 using
nc:
	nc -t 127.0.0.1 6502

KEGS will start printing "RING" every second, for 4 seconds.  You need to
type "ATA" within that time, and then you will connect.  Alternatively,
you can tell the virtual modem to automatically pick up after 2 rings:

	ATS0=2

KEGS will answer after 2 rings.  ATS0=0 means never answer.

Summary of AT commands:
	ATA	answer if RING'ing
	ATZ	Reset all settings to default (which is verbose)
	ATE1	Turns on echo
	ATE0	Turns off echo
	ATV1	Turns on verbose
	ATV0	Turns off verbose
	ATO	Goes online (if you did +++ and want to stay connected)
	ATH	Hang up
	ATDThostname:port_num	Opens connection to "hostname" on port_num
	ATS0=2	Answers after 2 rings without needing ATA.

On Windows, when KEGS tries to open this incoming socket, you'll
need to enable it and click Unblock to the dialog that Windows pops up.
This may happen when you first start KEGS since the virtual modem is now
on by default.

  Remote IP
  ---------

In this mode, KEGS tries to open a socket to the address in "Remote IP",
to the port in "Remote Port".  If you set Remote IP to your printer,
and Remote Port to 9100, then KEGS will open a connection to your printer.

  Use incoming port 6501 (or 6502)
  --------------------------------

KEGS also supports an older, simpler socket interface where it accepts any
connection to port 6501 (or 6502 for slot 2). In KEGS, from APPLESOFT, if
you PR#1, all output will then be sent to socket port 6501.  You can see this
data by connecting to the port using nc:  In another terminal window, do:
"nc -t localhost 6501" and then you will see all the output going to the
"printer".

Under APPLESOFT, you can PR#1 and IN#1.  This gets input from the socket
also.  You can type in the telnet window, it will be sent on to the
emulated IIgs.  You may want to edit the Serial Configuration option
"Serial Mask" to "Mask off high bit" to make the PR#1 output easier to
handle (Apple II's set the high-bit in most bytes output, which is not
what any modern computer wants).

You can "print" from BASIC by using something like PR#1 in KEGS and
"nc -t localhost 6501 | tee file.out" in another window.

