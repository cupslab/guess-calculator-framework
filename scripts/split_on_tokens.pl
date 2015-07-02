#!/usr/bin/perl

# Split the password in the input file using the structure break symbol (\x01) and 
# write to separate files based on token length
# Operates on a plaintext training file (e.g. word-freq format)
# Output plaintext wordfreq format to STDOUT
#
# Perl can't read the hex float format of a word-freq file, but we can output the freq raw and rely on the wordfreq processor.
# I still prefer to use Perl for this because Python regex ranges include Unicode and I haven't found a way around this.
# 
# Written by Michelle Mazurek, 6/10/2013
# Modified by SK
use strict;
use warnings;
use bytes;

# Load Perl modules
use Cwd 'abs_path';

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
    print "split_on_tokens.pl <pwd file> <name of existing folder in which to write files>\n";
    print "<pwd file> must be in plaintext wordfreq format and final output will also be in plaintext wordfreq format\n\n"
}

my %openfiles = ();
sub write_to_folder {
    my $pwd = shift;
    my $pwdlen = shift;
    my $dirname = shift;
    die "Not enough arguments to write_to_folder!\n" if (!defined $dirname);

    if (exists $openfiles{$pwdlen}) {
        print {$openfiles{$pwdlen}} $pwd;
    } else {
        # Open filehandle
        open $openfiles{$pwdlen}, '>', "$dirname/$pwdlen.txt" or die("Cannot open $dirname/$pwdlen.txt");
        print {$openfiles{$pwdlen}} $pwd;
    }
}

my $dirname;
if(@ARGV == 1) {
    $dirname = abs_path("./splitfiles");
    if (-d $dirname) {
        print STDERR "Clearing splitfiles directory\n";
        system("rm -f $dirname/*");
    } else {
        system("mkdir $dirname");
    }
}
elsif (@ARGV == 2) {
    $dirname = abs_path($ARGV[1]);    
}
else {
    usage();
    die "Wrong number of arguments!";
}

die "Unable to find directory $dirname" if !(-d $dirname);

my $source = shift @ARGV;
open my $fh, '<', $source or die("Cannot open $source!");
while(my $line = <$fh>) {
    chomp $line;
    my @fields = split /\t/, $line;
    my $pwd = $fields[0];
    my $incount = $fields[1];
    my $inID = $fields[2];
    debug_print "\nSplitting $pwd:";

    # Match only on ASCII letters
    while($pwd =~ /([^\x01]+)/g) {
        my $found = $1;
        my $pwdlen = length($found);
        debug_print "$found length:$pwdlen\n";
        write_to_folder "$found\t$incount\t$inID\n", $pwdlen, $dirname;
    }
}

close $fh;
foreach my $fh (values %openfiles) {
    close $fh;
}
