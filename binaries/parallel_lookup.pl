#!/usr/bin/perl
###############################################################################
# parallel_lookup.pl
# This script manages the parallel lookup of test set passwords.
# It has almost same structure as parallel_gentable, and the parallel sort in
#   single_run, so in the future it might be worth refactoring all three.
#
use strict;
use warnings;

# Load Perl modules
use Getopt::Std;
use POSIX qw(_exit);
# Load local modules
use ProcessHandler;    # uses POSIX ":sys_wait_h"
use MiscUtils;

# Version
my $VERSION = "0.4";    # Fri Apr 11 11:45:39 2014

sub print_usage {
  # Print usage if arguments are incorrectly specified
  my ($arg) = @_;
  print STDERR "missing $arg \n" if defined $arg;

  print STDERR << "EOF";
  parallel lookup script v$VERSION

  $0 [-h][-p][-D][-u][-b] -n ? -L file1 -I file5

  this script requires several arguments:
    -L lookup_table_file
    -I input_passwords_file  absolute path to file containing passwords to be looked up, in three-column lookup format
    -n # set the number of cores to utilize

  This script also requires that LookupGuessNumbers, lookup file, and grammar directories be in the current directory!

  there are also several optional arguments:
    -h   this help
    -p   don't split input lookup file (assumes file is already split)
    -D   don't delete split files at the end
    -u   bias output guess numbers up (away from 0) on tie
    -b   bias output guess numbers down (towards 0) on tie

  example:
    $0 -n 62 -L lookuptable-1e-15 -I condition-name.test

EOF

  exit 1;
} ## end sub print_usage

#####
##### Initialization
#####
# Process command-line options
# Store options in a hash with default values
my $options = {
  cores       => 1,
  shouldSplit => 1,
  deleteMode  => 1,
  biasUp      => 0,
  biasDown    => 0
};
my %opts;
getopts('L:I:hpD:ubn:', \%opts);

# Check that required arguments are specified and that files exist
print_usage() if defined $opts{h};
my @reqArguments = (
  [qw/-L L lookuptablefile/],
  [qw/-I I testsetfile/]
);
foreach my $reqarg (@reqArguments) {
  print_usage(@$reqarg[0]) if !defined $opts{ @$reqarg[1] };
  $options->{ @$reqarg[2] } = $opts{ @$reqarg[1] };
  MiscUtils::test_exist($options->{ @$reqarg[2] });
}

# Process optional arguments
my @optArguments = (
  [qw/n cores ^\\d+$/]
);
foreach my $optarg (@optArguments) {
  if (defined $opts{ @$optarg[0] }) {
    if ($opts{ @$optarg[0] } =~ /@$optarg[2]/) {
      $options->{ @$optarg[1] } = $opts{ @$optarg[0] };
    }
    else {
      die
        "Option @$optarg[0] was malformed!  Read in as $opts{@$optarg[0]}\n";
    }
  }
}

# Set config variables for those options which have no associated value or when we want to store that a value was specified (as in number of cores)
my @optBooleans = (
  [qw/p shouldSplit 0/],
  [qw/D deleteMode 0/],
  [qw/u biasUp 1/],
  [qw/b biasDown 1/]
);
foreach my $optbool (@optBooleans) {
  $options->{ @$optbool[1] } = @$optbool[2] if defined $opts{ @$optbool[0] };
}

# Check that the required binary exists
my @supportBinaries = ("LookupGuessNumbers");
foreach my $binary (@supportBinaries) {
  MiscUtils::test_exec($binary);
}

# Strip path from test set filename because it is reused in later filenames
($options->{inputfilename} = $options->{testsetfile}) =~ s{.*/}{};

#####
##### Main section
#####

my $start_overall = time();
print STDERR "\nScript started at $start_overall ("
  . MiscUtils::human_print_seconds($start_overall) . ")\n";
print STDERR "using $options->{cores} cores for parallel portions\n";
print STDERR << "EOF";
INPUT FILES:
$options->{lookuptablefile}
$options->{testsetfile}

EOF

# Subprocesses are handled by ProcessHandler
my $prochandler = ProcessHandler->new();
# On SIGCHLD, call REAPER to make sure any zombie processes are properly killed
$SIG{CHLD} = sub { $prochandler->REAPER() };

# On SIGINT or SIGTERM, kill all subprocesses and exit
$SIG{INT} = \&tree_killer;
$SIG{TERM} = \&tree_killer;
sub tree_killer {
  print STDERR "Caught SIGINT or SIGTERM! Killing all subprocesses and aborting!\n";
  $prochandler->killAll();
  exit(1);
}

# Finally, kill child processes on termination (die or exit) lest they continue to fill the disk
END {
  $prochandler->killAll();
}



print STDERR "     === Parallel Lookup ===\n";

my $start_run = time();
my $cmd       = "";
if ($options->{shouldSplit}) {
  print STDERR "Creating lookuppieces directory....\n";
  MiscUtils::deleteFiles("lookuppieces", $options->{deleteMode});
  system("mkdir lookuppieces") == 0
    or die "Error: unable to make lookuppieces directory";
  print STDERR "      ======splitting======    \n";
  print STDERR
    "Splitting in $options->{cores} chunks to run on $options->{cores} cores\n";
  $cmd = "cd lookuppieces && " .
    "split -a 6 -n l/$options->{cores} " .
    "\"$options->{testsetfile}\" tolookup-split.";
  print STDERR "executing: $cmd \n";
  my $start_split = time();
  system($cmd) == 0
    or die
    "Error: splitting test set file returned nonzero errorlevel!!  Aborting.\n";
  my $end_split  = time();
  my $split_time = $end_split - $start_split;
  print STDERR "\nTest set file split took $split_time seconds ("
    . MiscUtils::human_print_seconds($split_time) . ")\n";
} ## end if ($options->{shouldSplit...})
else {
  print STDERR
    "Skipping split. Expecting split test set files in lookuppieces directory...\n";
}

my @splitfiles = `find . | grep -o "tolookup-split.*\$"`;
print STDERR "there are " . scalar(@splitfiles) . " files to process\n";

print STDERR "    =======processing (<= $options->{cores} processes) =====\n";
my $start_lookup = time();

my $binaryArguments = "";
if ($options->{biasUp}) {
    $binaryArguments = " -bias-up ";
    print STDERR "Biasing output up\n";
} elsif ($options->{biasDown}) {
    $binaryArguments = " -bias-down ";
    print STDERR "Biasing output down\n";
}

my $mockcmd = "./$supportBinaries[0] " .
  "-lfile $options->{lookuptablefile} " .
  "-pfile lookuppieces/tolookup-split.?? " .
  "$binaryArguments" .
  "> lookuppieces/lookupedresults-split.??";
print STDERR "executing: '$mockcmd' \n";

# Create a new process for each core
for my $i (0 .. (scalar(@splitfiles) - 1)) {
  my $LookupPiecePID = fork();
  if ($LookupPiecePID) {
    # Parent process marks this child as alive using its pid
    #   (this allows the parent to terminate all children in the event of a SIGINT)
    #     $lock->down;
    $prochandler->setAlive($LookupPiecePID);
    #			$lock->up;
  }
  elsif ($LookupPiecePID == 0) {
    # Child process runs support binary
    $0 = "lookup process $i";
    setpgrp $$, 0;
    my $infile = $splitfiles[$i];
    if (defined $infile) {
      chomp $infile;
      my ($outname) = $infile =~ m/tolookup-(.*)/;
      if (-e "lookuppieces/$infile" && -s "lookuppieces/$infile") {
        my $cmd = "./$supportBinaries[0] " .
          "-lfile $options->{lookuptablefile} " .
          "-pfile lookuppieces/$infile " .
          "> lookuppieces/lookedupresults-$outname";
        print STDERR "In child #" . $i . ": ";
        print STDERR "executing $cmd\n";
        my $returnstatus = system($cmd);
        if ($returnstatus != 0 && $returnstatus != 65280) {
          print STDERR "system $cmd returned errorcode: $returnstatus\n";
          die "exec of $cmd returned errorcode failed with: $!\n";
        }
        # If previous command returned successfully, use exec to kill this
        # child without running END block
        exec $^X => -eexit;
      }
      else {
        die "$infile doesn't exist!\n";
      }
    }
    exit;
  } ## end elsif ($LookupPiecePID ==...)
  else {
    die "fork failed: $!\n";
  }
} ## end for my $i (0 .. scalar(...))

print STDERR "\nWatching the children work...\n";
while ((my $running = $prochandler->aliveCount()) > 0) {
  $prochandler->checkChildErrorAndExit();
  print STDERR "$running children still working\n";
  sleep 60;
}

my $end_lookup  = time();
my $lookup_time = $end_lookup - $start_lookup;
print STDERR "\nParallel lookup took $lookup_time seconds ("
  . MiscUtils::human_print_seconds($lookup_time) . ")\n";

# Merge lookup results to stdout
$cmd = "cat lookuppieces/lookedupresults-*";
print STDERR "executing: $cmd \n";
my $start_merge = time();
system($cmd) == 0 or die "Merging lookup result pieces failed!!\n";
my $end_merge  = time();
my $merge_time = $end_merge - $start_merge;
print STDERR "\nLookup results merge took $merge_time seconds ("
  . MiscUtils::human_print_seconds($merge_time) . ")\n";

my $end_run  = time();
my $run_time = $end_run - $start_run;
print STDERR "\nTotal parallel lookup took $run_time seconds ("
  . MiscUtils::human_print_seconds($run_time) . ")\n";

# Cleanup - delete split files
MiscUtils::deleteFiles("lookuppieces", $options->{deleteMode});

my $end_overall = time();
my $run_overall = $end_overall - $start_overall;
print STDERR "\nEntire lookup took $run_overall seconds ("
  . MiscUtils::human_print_seconds($run_overall) . ")\n";

# Kill processes, flush I/O, and then exit -- I haven't found any other way to
# ensure that a 0 is returned.
$prochandler->killAll();
$| = 1;
_exit(0);
