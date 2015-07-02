#!/usr/bin/perl
#
# Testing filter
#
# Produce the subset of a password list that passes basic6
# Operates on a plaintext password file in single-column format
# Output passing lines to STDOUT
#
# Optional last argument = file to put non-selected passwords into
#
# Written by Saranga Komanduri
use strict;
use warnings;

# I prefer to use Perl for filtering because of this directive.  Python regex ranges include Unicode
# and we treat Unicode as symbols for now.
#
use bytes;

my $DEBUG = 0;

sub debug_print {
  my $arg = shift @_;
  #print if debug enabled
  if ($DEBUG) {
    print STDERR $arg;
  }
}

sub usage {
  print
    "filter_basic6_test.pl <plaintext pwd file> [outfile for non-selected passwords]\n";
}

my $use_outfile;
if (@ARGV == 1) {
  $use_outfile = 0;
}
elsif (@ARGV == 2) {
  $use_outfile = 1;
}
else {
  usage();
  die "Wrong number of arguments!";
}

my $source = shift @ARGV;
my $out;
if ($use_outfile) {
  my $outfile = shift @ARGV;
  open $out, '>', $outfile or die "Couldn't open $outfile for write!";
}

open my $file, '<', $source or die("Cannot open $source!");
PWD_LOOP:
while (my $pwd = <$file>) {
  chomp $pwd;
  debug_print "Testing $pwd";

  # must be at least 8 chars
  my $length = length($pwd);
  debug_print "($length): ";
  if ($length < 6) {
    debug_print "too short!\n";
    if ($use_outfile) { print $out "$pwd\n"; }
    next PWD_LOOP;
  }

  # else we passed!
  debug_print "PASS!\n";
  print STDOUT "$pwd\n";
} ## end PWD_LOOP: while (my $pwd = <$file>)

if ($use_outfile) {
  close $out;
}
close $file;
