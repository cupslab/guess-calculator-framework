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
  print "simpleguess.pl <pwd source file> <test file>\n";
  print
    "<pwd source file> can be in plaintext format or gzipped wordfreq format\n";
  print
    "<test file> must be in plaintext, one password per line\n\n";
}

if (@ARGV != 2) {
  usage();
  die "Wrong number of arguments!";
}
my $source   = shift @ARGV;
my $testfile = shift @ARGV;

# Load passwords from test file into memory with frequencies
my %testpasswords;
open my $fh, "-|", "cat \"$testfile\"";
while (my $line = <$fh>) {
  chomp $line;
  $testpasswords{$line} ||= 0;
  $testpasswords{$line} += 1;
}
close($fh);

# Process the source file
my ($dir, $name, $ext) = fileparse($source, qw(.gz));
my $cmd = "gunzip -c \"$source\"";
# Run process_wordfreq over the file if it is not gzipped
unless ($ext) {
  $cmd = "./process_wordfreq.py \"$source\"";
}
# Replace frequencies with line numbers
$cmd .= ' | awk \'BEGIN { OFS="\t"} { print $1 "\t" NR }\'';

# Find guessnumbers for the test passwords
open $fh, "-|", $cmd or die;
while (my $line = <$fh>) {
  chomp $line;
  my @fields      = split /\t/, $line;
  my $pwd         = $fields[0];
  my $guessnumber = $fields[1];
  if (exists $testpasswords{$pwd}) {
    for (my $i = 0; $i < $testpasswords{$pwd}; $i++) {
      print "nouser\tsimpleguess\t$pwd\t0x1p-1\tNA\t$guessnumber\tSG\n";
    }
    delete $testpasswords{$pwd};
  }
}
my $totalcount = $.;
close($fh);

# Now print all the test passwords that weren't found
while ((my $pwd, my $count) = each %testpasswords) {
  for (my $i = 0; $i < $count; $i++) {
    print "nouser\tsimpleguess\t$pwd\t0x1p-1\tNA\t-2\tSG\n";
  }
}

# Output totalcounts to stderr
print STDERR "(standard input):Total count\t$totalcount\n";
1;
