#!/usr/bin/perl -w

use strict;
use constant SIZEOF_HEADER => 6;
use constant SIZEOF_ENTRY => 16;

use Getopt::Long;

sub load_png {

	my ($file, $width, $height) = @_;

	print "Opening $file\n";
	open(my $io, "<", $file) or return "";
	binmode $io;

	local $/;
	my $data = <$io>;

	my @header = unpack("C[8] NN NNC[5] N", $data);
	close($io);

	# png header
	return "" unless $header[0] == 0x89;
	return "" unless $header[1] == 0x50;
	return "" unless $header[2] == 0x4e;
	return "" unless $header[3] == 0x47;
	return "" unless $header[4] == 0x0d;
	return "" unless $header[5] == 0x0a;
	return "" unless $header[6] == 0x1a;
	return "" unless $header[7] == 0x0a;

	# ihdr
	return "" unless $header[8] == 0x0d;
	return "" unless $header[9] == 0x49484452;
	$$width = $header[10];
	$$height = $header[11];

	# bit depth, color type, compression method, filter method, interface method
	# crc

	return $data;
}

GetOptions();




my $count = scalar @ARGV;

die "Usage: make_ico file.png ..." unless $count;

open(my $ico, ">", "icon.ico") or die "unable to open icon.ico";
binmode $ico;

my $height = 0;
my $width = 0;
my $size = 0;

my $offset = SIZEOF_HEADER + SIZEOF_ENTRY * $count;
my $header = pack("vvv", 0, 1, $count);

my $entry = "";

seek $ico, $offset, 0;

foreach my $file (@ARGV) {
	my $data = load_png($file, \$height, \$width);
	my $size = length($data);

	die "Icon too large ($file)" if ($height > 256 || $width > 256);
	$height = 0 if ($height == 256);
	$width = 0 if ($width == 256);

	$entry = $entry . pack("CCCCvvVV", $width, $height, 0, 0, 0, 0, $size, $offset);
	$offset += $size;

	print $ico $data;
}

seek $ico, 0, 0;
print $ico $header;
print $ico $entry;

close($ico);
exit 0;

