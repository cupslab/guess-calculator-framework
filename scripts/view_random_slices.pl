#!/usr/bin/perl

# Utility script to see random slices of a text file -- used for spot checking a lookup table
sub usage {
   print STDERR "$0 <filename> [num slices: default = 20] [slice size: default = 200 bytes]\n";
}

if (@ARGV < 1) { usage(); die "Wrong number of arguments!"; }

my $filename = shift @ARGV;
my $filesize = -s $filename;

my $n = shift @ARGV;
if (!defined $n) {
  $n = 20;
}

my $h = shift @ARGV;
if (!defined $h) {
  $h = 200;
}

my @randvals = map { int(rand($filesize)) } ( 1..$n );

foreach $fpos (@randvals) {
  print "\n";
  system("tail -c +$fpos \"$filename\" | head -c $h");
  print "\n";
}

