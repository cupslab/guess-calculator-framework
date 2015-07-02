#!/usr/bin/perl
#
# Training filter
#
# Produce the subset of a password list that passes 4class8
# Operates on a plaintext file in wordfreq format
# Output plaintext wordfreq format for passwords that pass filter
#
# Optional last argument = file to put non-selected passwords into
#
# Written by Michelle Mazurek, 6/10/2013
# Modified by SK
use strict;
use warnings;

# I prefer to use Perl for filtering because of this directive.  Python regex ranges include Unicode
# and we treat Unicode as symbols for now.
#
use bytes;

my $DEBUG = 0;

sub debug_print {
  my $arg = shift @_;
  # Print if debug enabled
  if ($DEBUG) {
    print STDERR $arg;
  }
}

sub usage {
  print
    "filter_4class8_train.pl <pwd file> [outfile for non-selected passwords]\n";
  print
    "<pwd file> must be in plaintext wordfreq format and final output will also be in plaintext wordfreq format\n\n"
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
while (my $line = <$file>) {
  chomp $line;
  my @fields = split /\t/, $line;
  my $pwd = $fields[0];
  debug_print "Testing $pwd";

  # must be at least 8 chars
  my $length = length($pwd);
  debug_print "($length): ";
  if ($length < 8) {
    debug_print "too short!\n";
    if ($use_outfile) { print $out "$line\n"; }
    next PWD_LOOP;
  }

  # must contain lower, upper, digit, symbol
  if (!($pwd =~ /[a-z]/)) {
    debug_print "no lower!\n";
    if ($use_outfile) { print $out "$pwd\n"; }
    next PWD_LOOP;
  }
  if (!($pwd =~ /[A-Z]/)) {
    debug_print "no upper!\n";
    if ($use_outfile) { print $out "$pwd\n"; }
    next PWD_LOOP;
  }
  if (!($pwd =~ /[0-9]/)) {
    debug_print "no digit!\n";
    if ($use_outfile) { print $out "$pwd\n"; }
    next PWD_LOOP;
  }
  if (!($pwd =~ /[^a-zA-Z0-9]/)) {
    debug_print "no symbol!\n";
    if ($use_outfile) { print $out "$pwd\n"; }
    next PWD_LOOP;
  }

  # else we passed!
  debug_print "PASS!\n";
  print STDOUT "$line\n";
} ## end PWD_LOOP: while (my $line = <$file>)

if ($use_outfile) {
  close $out;
}
close $file;
