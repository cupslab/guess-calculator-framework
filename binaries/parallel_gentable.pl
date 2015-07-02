#!/usr/bin/perl
###############################################################################
# parallel_gentable.pl
# This script manages the parallel generation of the raw table of guesses from
#   the PCFG specification, up to the given probability threshold (cutoff).
#
# Written by Saranga Komanduri and Tim Vidas
use strict;
use warnings;

# Load Perl modules
use Getopt::Std;
use POSIX qw(_exit);
# Load local modules
use ProcessHandler;    # uses POSIX ":sys_wait_h"
use MiscUtils;

# Version
my $VERSION = "0.8";    # Mon Oct  6 11:03:27 2014

sub print_usage {
  # Print usage if arguments are incorrectly specified
  my ($arg) = @_;
  print STDERR "missing $arg \n" if defined $arg;

  print STDERR << "EOF";
  parallel gentable script v$VERSION

  $0 [-h][-p][-D][-b][-s][-A] -n ? -c cutoff

  this script requires several arguments:
    -c cutoff
    -n # set the number of cores to utilize 

  This script also requires that GeneratePatterns and the grammar directory be in the current directory!

  there are also several optional arguments:
    -h   this help
    -s   generate strings from the grammar in probability order through parallel generation, sorting, and then removing the probability field
    -A   generate strings from the grammar in probability order through generation, lookup against all structures to prevent duplicates, sorting, and then removing the probability field (this operation cannot be parallelized)
    -b   perform deterministic randomization (useful for testing)
    -p   don't split input structures file (assumes file is already split)
      -D   don't delete split files at the end

  example:
    $0 -n 62 -c 1e-16

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
  stableMode  => 0,
  stringsMode => 0,
  accuStrMode => 0
};
my %opts;
getopts('c:hbsApD:n:', \%opts);

# Check that required arguments are specified
print_usage() if defined $opts{h};
my @reqArguments = (
  [qw/-c c cutoff/]
);
foreach my $reqarg (@reqArguments) {
  print_usage(@$reqarg[0]) if !defined $opts{ @$reqarg[1] };
  $options->{ @$reqarg[2] } = $opts{ @$reqarg[1] };
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
  [qw/s stringsMode 1/],
  [qw/A accuStrMode 1/],
  [qw/D deleteMode 0/],
  [qw/b stableMode 1/]
);
foreach my $optbool (@optBooleans) {
  $options->{ @$optbool[1] } = @$optbool[2] if defined $opts{ @$optbool[0] };
}

# Force certain conditions if accuStrMode is set
# We cannot parallelize because all structures must be available in order to sum across tokenizations
if ($options->{accuStrMode}) {
  print STDERR "Accurate strings mode specified.  Disabling parallel generation.\n";
  $options->{stringsMode} = 1;
  $options->{cores} = 1;
}

# Check that the required binary exists
my @supportBinaries = ("GeneratePatterns");
if ($options->{stringsMode}) {
  @supportBinaries = ("GenerateStrings");
}
foreach my $binary (@supportBinaries) {
  MiscUtils::test_exec($binary);
}

#####
##### Build raw table
#####

my $start_overall = time();
print STDERR "\nScript started at $start_overall ("
  . MiscUtils::human_print_seconds($start_overall) . ")\n";
print STDERR "using $options->{cores} cores for parallel portions\n";
print STDERR "Probability threshold: $options->{cutoff}\n\n";

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



print STDERR "     === Parallel build raw table ===\n";

my $start_run = time();
my $cmd       = "";
if ($options->{shouldSplit}) {
  print STDERR "Creating structure pieces directory....\n";
  MiscUtils::deleteFiles("structurepieces", $options->{deleteMode});
  system("mkdir structurepieces") == 0
    or die "Error: unable to make structurepieces directory";
  print STDERR "      ======splitting======    \n";
  print STDERR
    "Splitting in $options->{cores} chunks to run on $options->{cores} cores\n";
  my $start_split = time();

  my $structuresfile = "grammar/nonterminalRules.txt";
  print STDERR 
    "Finding blank line in structures file and splitting in three parts...\n";
  open(my $fh, "<", $structuresfile)
    or die "Unable to open structures file in parallel_gentable -$?!\n";
  open(my $sheadfh, ">", "structurepieces/structurefilehead")
    or die "Unable to open structurefilehead for writing in parallel_gentable -$?!\n";
  open(my $stailfh, ">", "structurepieces/structurefiletail")
    or die "Unable to open structurefiletail for writing in parallel_gentable -$?!\n";
  open(my $smiddlefh, ">", "structurepieces/structures")
    or die "Unable to open structures for writing in parallel_gentable -$?!\n";
  my $blanklinectr = 0;
  # Print first line to head file, everything up to the first blank line to the
  #   middle file, and print the rest to the tail file.
  while (my $line = <$fh>) {
    if ($. == 1) {
      print $sheadfh $line;
    } else {
      $blanklinectr++ if ($line =~ /^$/);
      if ($blanklinectr == 0) {
        print $smiddlefh $line;
      } else {
        print $stailfh $line;
      }
    }
  }
  close($fh);
  close($sheadfh);
  close($stailfh);
  close($smiddlefh);

  # TODO: Just do this all in Perl
  print STDERR "Shuffling structures\n";
  my $stableopt = "";
  $stableopt = "--random-source=../random.bits" if ($options->{stableMode});
  system(
    "cd structurepieces && " .
      "shuf " .
      "$stableopt " .    # Use a static source of randomness for deterministic results
      "structures " .    # This must match the filename (but not path) for $smiddlefh above
      "> shufstructures.txt"
    ) == 0
    or die
    "Error: shuf structures failed!  Maybe your version of shuf does not support the --random-source parameter?\n";
  $cmd =
    "cd structurepieces && split -a 6 -n l/$options->{cores} shufstructures.txt structure-split.";
  print STDERR "executing: $cmd \n";
  system($cmd) == 0
    or die
    "Error: splitting shufstructures returned nonzero errorlevel!!  Aborting.\n";

  my @splitfiles = `find . | grep -o "structure-split.*\$"`;
  chomp(@splitfiles);
  foreach my $splitfile (@splitfiles) {
    $cmd =
      "cd structurepieces && " .
      "cat structurefilehead \"$splitfile\" structurefiletail > foo && " .
      "cp foo \"$splitfile\"";
    print STDERR "executing: $cmd \n";
    system($cmd) == 0
      or die
      "Error: fixing shuffled structures returned nonzero errorlevel!!  Aborting.\n";
  }

  my $end_split = time();
  my $split_time = $end_split - $start_split;
  print STDERR "\nStructure file split took $split_time seconds ("
    . MiscUtils::human_print_seconds($split_time) . ")\n";
} ## end if ($options->{shouldSplit...})
else {
  print STDERR
    "Skipping split. Expecting split structure files in structurepieces directory...\n";
}

my @splitfiles = `find . | grep -o "structure-split.*\$"`;
print STDERR "there are " . scalar(@splitfiles) . " files to process\n";

print STDERR "    =======processing (<= $options->{cores} processes) =====\n";
my $start_gen = time();

my $accuswitch = "";
$accuswitch = "-accupr" if $options->{accuStrMode};
my $mockcmd = "./$supportBinaries[0] " .
  "-cutoff $options->{cutoff} $accuswitch " .
  "-sfile structurepieces/structure-split.?? " .
  "> structurepieces/rawtablepieces-split.??";
print STDERR "Will execute: '$mockcmd' \n";

# Create a new process for each core
# Unlike single_run, there is no need to have more batches than cores
#   we just assign a core to each split piece and let it finish
for my $i (0 .. (scalar(@splitfiles) - 1)) {
  my $GenRawTablePID = fork();
  if ($GenRawTablePID) {
    # Parent process marks this child as alive using its pid
    #   (this allows the parent to terminate all children in the event of a SIGINT)
    #			$lock->down;
    $prochandler->setAlive($GenRawTablePID);
    #			$lock->up;
  }
  elsif ($GenRawTablePID == 0) {
    # Child process runs support binary
    $0 = "gentable process $i";
    setpgrp $$, 0;
    my $infile = $splitfiles[$i];
    if (defined $infile) {
      chomp $infile;
      my ($outname) = $infile =~ m/structure-(.*)/;
      if (-e "structurepieces/$infile" && -s "structurepieces/$infile") {
        my $cmd = "./$supportBinaries[0] " .
          "-cutoff $options->{cutoff} $accuswitch " .
          "-sfile structurepieces/$infile " .
          "> structurepieces/rawtablepieces-$outname";
        print STDERR "In child #" . $i . " (pid: $$): ";
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
  } ## end elsif ($GenRawTablePID ==...)
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

my $end_gen  = time();
my $gen_time = $end_gen - $start_gen;
print STDERR "\nParallel build rawtable took $gen_time seconds ("
  . MiscUtils::human_print_seconds($gen_time) . ")\n";

# Merge lookup results to stdout
$cmd = "cat structurepieces/rawtablepieces-*";
if ($options->{stringsMode}) {
  $cmd = "sort -T . -grs structurepieces/rawtablepieces-* | cut -s -f2" 
}
print STDERR "executing: $cmd \n";
my $start_merge = time();
system($cmd) == 0 or die "Merging raw table pieces failed!!\n";
my $end_merge  = time();
my $merge_time = $end_merge - $start_merge;
print STDERR "\nTable pieces merge took $merge_time seconds ("
  . MiscUtils::human_print_seconds($merge_time) . ")\n";

my $end_run  = time();
my $run_time = $end_run - $start_run;
print STDERR "\nTotal parallel build raw table took $run_time seconds ("
  . MiscUtils::human_print_seconds($run_time) . ")\n";

# Cleanup - delete split files
MiscUtils::deleteFiles("structurepieces", $options->{deleteMode});

my $end_overall = time();
my $run_overall = $end_overall - $start_overall;
print STDERR "\nEntire run took $run_overall seconds ("
  . MiscUtils::human_print_seconds($run_overall) . ")\n";

# Kill processes, flush I/O, and then exit -- I haven't found any other way to
# ensure that a 0 is returned.
$prochandler->killAll();
$| = 1;
_exit(0);
