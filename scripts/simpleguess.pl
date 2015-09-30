#!/usr/bin/perl
#
# This script emulates the Weir 2010 approach to guessing passwords. It takes a
# single file as a training set, sorts it by frequency if needed, and uses it to
# generate guess numbers for the test set.  It is intended to be used on simple
# password lists where we can grep through the file instead of binary search,
# and small test files that can be stored in RAM. Since no new passwords are
# generated, the maximum guess number is the number of distinct passwords in the
# training set.
#
# The output is in lookup-result format, so that it is compatible with the
# plotting tools in PlotResults.R
#
# Written by Saranga Komanduri, 9/12/2015
use strict;
use warnings;
use File::Basename;

# Load local modules
use lib 'binaries';
use MiscUtils;

my $VERSION = "0.1";    # Sat Sep 12 22:19:28 2015

sub usage {
  print "cat <pwd source file> | simpleguess.pl <test file> [policy name]\n";
  print
    "<pwd source file> must be in sorted plaintext wordfreq format OR already sorted with one password per line and no tab characters in passwords\n";
  print
    "<test file> must be in plaintext, one password per line\n";
  print
    "[policy name] is an optional string to use in the policy name field of the lookup results\n\n";
}

if (@ARGV < 1 || @ARGV > 3) {
  usage();
  die "Wrong number of arguments!";
}
my $testfile = shift @ARGV;
my $conditionname = shift @ARGV;
if (!$conditionname) {
  $conditionname = "simpleguess";
}

# Load passwords from test file into memory with frequencies
my %testpasswords;
open my $fh, "<", $testfile;
while (my $line = <$fh>) {
  chomp $line;
  $testpasswords{$line} ||= 0;
  $testpasswords{$line} += 1;
}
close($fh);

# Process the source file
my $guessnumber = 1;
while (my $line = <STDIN>) {
  chomp $line;
  my @fields      = split /\t/, $line;
  my $pwd         = $fields[0];
  # Ignore the frequency field

  if (exists $testpasswords{$pwd}) {
    for (my $i = 0; $i < $testpasswords{$pwd}; $i++) {
      print "nouser\t$conditionname\t$pwd\t0x1p-1\tNA\t$guessnumber\tSG\n";
    }
    delete $testpasswords{$pwd};
  }

  $guessnumber += 1;
}
my $totalcount = $.;

# Now print all the test passwords that weren't found
while ((my $pwd, my $count) = each %testpasswords) {
  for (my $i = 0; $i < $count; $i++) {
    print "nouser\t$conditionname\t$pwd\t0x1p-1\tNA\t-2\tSG\n";
  }
}

# Output totalcounts to stderr
print STDERR "(standard input):Total count\t$totalcount\n";
1;
