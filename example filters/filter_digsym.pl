#!/usr/bin/perl
#
# Training filter
#
# Extract the digit strings and symbol strings from the input and lowercase them
# Operates on a plaintext file in wordfreq format
# Output plaintext wordfreq format to STDOUT
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
  $arg = shift @_;
  #print if debug enabled
  if ($DEBUG) {
    print STDERR $arg;
  }
  #otherwise nothing
}

sub usage {
  print "filter_digsym.pl <pwd file>\n";
  print
    "<pwd file> must be in plaintext wordfreq format and final output will also be in plaintext wordfreq format\n\n"
}

if (@ARGV != 1) {
  usage();
  die "Wrong number of arguments!";
}

my $source = shift @ARGV;
open my $file, '<', $source or die("Cannot open $source!");
while (my $line = <$file>) {
  chomp $line;
  my @fields  = split /\t/, $line;
  my $pwd     = $fields[0];
  my $incount = $fields[1];
  my $inID    = $fields[2];
  debug_print "\nTesting $pwd:";

  # Find strings of digits
  my $pwdcopy = $pwd;
  while ($pwd =~ /([0-9]+)/ag) {
    my $found = $1;
    debug_print "$found,";
    print "$found\t$incount\t$inID\n"
  }

  # Find strings of symbols
  while ($pwdcopy =~ /([^A-Za-z0-9]+)/ag) {
    my $found = $1;
    debug_print "$found,";
    print "$found\t$incount\t$inID\n"
  }
} ## end while (my $line = <$file>)

close $file;

