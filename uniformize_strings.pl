#!/usr/bin/perl

# If the input word is an alphabetic string, set it to a frequency of 1, so all
# strings have uniform frequency.  This emulates the original behavior of the
# Weir PCFG learner.  It is meant to be used after creating training and test
# corpora, but before learning the grammar.
# 
# Uses process_wordfreq.py
#
# Operates on a gzipped training file (e.g. word-freq format)
# Output gzipped wordfreq format to STDOUT
#
# Written by Michelle Mazurek, 6/10/2013
# Modified by SK
use strict;
use warnings;
use bytes;

use File::Temp;
use lib 'binaries';
use MiscUtils qw(run_cmd);

my $VERSION = "1.00";    # Tue Nov 11 22:58:32 2014
my $wordfreqprocessor = "./process_wordfreq.py";
my $DEBUG = 0;

sub debug_print {
    my $arg = shift @_;
    #print if debug enabled
    if($DEBUG) {
        print STDERR $arg;
    }
    #otherwise nothing
}

sub usage {
    print "uniformize_strings.pl <pwd file>\n";
    print "<pwd file> must be in gzipped wordfreq format and final output will also be in gzipped wordfreq format\n\n"
}

# Create temporary output file 1
( my $tmpFile = File::Temp->new(
    TEMPLATE => 'tempXXXXXX',
    SUFFIX   => '.gz'
    )
) or die "Could not create temporary file 1 - $@!\n";
open my $outfile, "| gzip -c > $tmpFile" or die "Couldn't open gzip to $tmpFile for write!";

# Step 1: Split terminal strings by break character
my $source = shift @ARGV;
system("cp $source oldterminals.gz");
open my $input, '-|', 'gzip', '-dc', $source or die("Cannot open $source!");
while(my $line = <$input>) {
    chomp $line;
    my @fields = split /\t/, $line;
    my $pwd = $fields[0];
    my $incount = $fields[1];
    my $inID = $fields[2];
    debug_print "\nTesting $pwd:";

    my @parts = split /\x01/, $pwd;
    foreach my $part (@parts) {
        # Match only on ASCII letters
        if($part =~ /^([A-Za-z]+)$/a) {
            my $found = lc $1;
            debug_print "$found,";
            print $outfile "$found\t1p+0\t$inID\n"
        } else {
            print $outfile "$part\t$incount\t$inID\n";
        }        
    }
}
close $input;
close $outfile;

# Step 2: Call process_wordfreq so terminals are uniqued
# Create temporary output file 2
( my $tmpFile2 = File::Temp->new(
    TEMPLATE => 'tempXXXXXX',
    SUFFIX   => '.gz'
    )
) or die "Could not create temporary file 2 - $@!\n";
run_cmd(
"$wordfreqprocessor -v -g \"$tmpFile\" \"$tmpFile2\""
);

# Step 3: Refilter for strings and set frequency to 1
open $input, '-|', 'gzip', '-dc', $tmpFile2 or die("Cannot open $tmpFile2!");
open $outfile, "| gzip -c > $source" or die "Couldn't open gzip to $source for write!";
while(my $line = <$input>) {
    chomp $line;
    my @fields = split /\t/, $line;
    my $pwd = $fields[0];
    my $incount = $fields[1];
    my $inID = $fields[2];
    debug_print "\nTesting $pwd:";

    my @parts = split /\x01/, $pwd;
    foreach my $part (@parts) {
        # Match only on ASCII letters
        if($part =~ /^([A-Za-z]+)$/a) {
            my $found = lc $1;
            debug_print "$found,";
            print $outfile "$found\t1p+0\t$inID\n"
        } else {
            print $outfile "$part\t$incount\t$inID\n";
        }        
    }
}
close $input;
close $outfile;
