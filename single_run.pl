###############################################################################
# single_run.pl
# This script processes one condition at a time through the guess calculator framework.
# It is normally called from iterate_experiments, but could be manually called.
#
# Written by Saranga Komanduri and Tim Vidas
use strict;
use warnings;

# Load Perl modules
use List::Util qw(min max);
use POSIX qw(floor _exit);
use POSIX qw(:signal_h :errno_h :sys_wait_h);

# Load local modules
use lib 'binaries';
use ProcessHandler;             # uses POSIX ":sys_wait_h"
use MiscUtils;
use SingleRunOptionsHandler;    # uses Getopt::Std

# Version
my $VERSION = "0.82";           # Thu Apr 10 11:41:50 2014

#####
##### Initialization
#####

# Process command-line options
my $options = SingleRunOptionsHandler->new({ VERSION => $VERSION });
# $options->{BLOCAL} = 0;  # uncomment this if running on Andrew machine
$options->process_command_line();
$options->set_cores();
$options->set_ulimit();

# Verify that required scripts are executable
my @supportScripts = ("scripts/GenerateTrainingSet.sh");
foreach my $script (@supportScripts) {
  MiscUtils::test_exec($script);
}

# Output current values for logging
print "\nRun started at $options->{start_overall} ("
  . MiscUtils::human_print_seconds($options->{start_overall}) . ")\n"
  .
  "using executable scripts:\n" .
  "@supportScripts\n" .
  "using cutoff: $options->{prThreshold}\n" .
  "using a maximum of $options->{cores} cores for parallel portions\n" .
  "using a maximum of $options->{memh} GB total memory for parallel portions\n"
  .
  "using a maximum of $options->{memsliceh} GB total memory for each sort slice\n"
  .
  "INPUT FILES:\n";

foreach my $opt (qw/structureFile terminalsFile/) {
  print "$options->{$opt}\n";
}
print "\n";

# Array of arrays of files that should be produced at each stage
my @supportBinaries = (
  "GeneratePatterns",  "sortedcountaggregator",
  "process.py",       "LookupGuessNumbers",
  "parallel_lookup.pl", "parallel_gentable.pl"
);
s|^|./$options->{rootName}/| for @supportBinaries;
my @newFiles = (
  ["$options->{rootName}.zip"],
  \@supportBinaries,
  [ "./$options->{rootName}/rawtable-" . $options->{prThreshold} ],
  [ "./$options->{rootName}/sortedtable-" . $options->{prThreshold} ],
  [ "./$options->{rootName}/lookuptable-" . $options->{prThreshold} ]
);

# Subprocess handling
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

#####
##### Build PCFG
#####

print "========== Learn PCFG specification ==========\n";

# Check for existing output files
my $thisstage = 0;
my $doStage   = !MiscUtils::checkStageFiles(@{ $newFiles[$thisstage] });
if ($doStage) {
  print "Building PCFG specification\n";
  my $cmd = "$supportScripts[0] " .
    "./binaries/ " .
    "\"$options->{structureFile}\" " .
    "\"$options->{terminalsFile}\" " .
    "$options->{rootName} " .
    "$options->{quantLevels} " .
    "$options->{quantIters} " .
    "$options->{generateUnseen}";
  print "executing: $cmd \n";
  die "could not fork: $!" unless defined(my $PCFGLearnPID = fork());

  if ($PCFGLearnPID) {
    # Parent for PCFGLearning process
    $prochandler->setAlive($PCFGLearnPID);
  }
  else {
    # Child
    setpgrp $$, 0;  # Set this child to be the head of a new process group, so
                    # if it needs to be killed by the parent due to low disk
                    # space, it will kill all children
    my $returnstatus = system($cmd);
    if ($returnstatus != 0 && $returnstatus != 65280) {
      print STDERR "system $cmd returned errorcode: $returnstatus\n";
      die "exec of $cmd returned errorcode failed with: $!\n";
    }
    # If previous command returned successfully, use exec to kill this
    # child without running END block
    exec $^X => -eexit;      
  }

  my $start_run = time();

  # Parent for PCFGLearning process will loop here until child is done, using the fact that we've set up a SIGCHLD handler
  until ($prochandler->isDead($PCFGLearnPID)) {
    $prochandler->checkChildErrorAndExit();
    print STDERR ".";    #status feedback while waiting
    if (time() - $options->{start_overall} > $options->{MAX_TIME}) {
      kill -15, $PCFGLearnPID;    #negative kills process group
      warn "\nreached maximum execution time "
        . MiscUtils::human_print_seconds($options->{MAX_TIME});
      exit($options->{exitcode});
    }
  }
  continue {
    sleep 1;
  }

  my $end_run  = time();
  my $run_time = $end_run - $start_run;
  print "\nLearning PCFG specification took $run_time seconds ("
    . MiscUtils::human_print_seconds($run_time) . ")\n";
} ## end if ($doStage)
else {
  print "Skipping PCFG learning\n";
}

MiscUtils::checkStageFiles(@{ $newFiles[$thisstage] })
  or die "Some stage $thisstage file is missing. Aborting!\n";

# If -o option was specified, quit right now
if ($options->{onlyMakeDictionaries}) {
  print "========== Only learning PCFG specification, exiting ==========\n";
  exit(0);
}

#####
##### Build lookup table
#####

print "\n========== Build Binaries ==========\n";

$thisstage = 1;
$doStage   = 1;
# Testing this stage is a little more complex than other stages
if (-d $options->{rootName}) {
  print STDERR "$options->{rootName} dir exists already...";
  # Now check for support binary files
  $doStage = !MiscUtils::checkStageFiles(@{ $newFiles[$thisstage] });
  if (!$doStage) {
    # We checked for existence, now remake them just to be sure
    foreach my $file (@{ $newFiles[$thisstage] }) {
      MiscUtils::test_exec($file);
    }
  }
}
if ($doStage) {
  # Wipe out folder because something is wrong with it
  MiscUtils::deleteFiles("$options->{rootName}", $options->{deleteMode});

  # Now rebuild from the zip file
  print STDERR
    "Creating $options->{rootName} directory for intermediate processing\n";
  my $cmd = "mkdir $options->{rootName}";
  system($cmd) == 0 or die "system of '$cmd' failed with $?";
  $cmd = "cd $options->{rootName}; unzip ../$options->{rootName}.zip";
  system($cmd) == 0 or die "system of '$cmd' failed with $?";
  print STDERR "building binaries...";
  $cmd = "cd $options->{rootName}; make clean > /dev/null && make > /dev/null";
  system($cmd) == 0 or die "system of '$cmd' failed with $?";
  # Ensure binaries are executable
  foreach my $file (@{ $newFiles[$thisstage] }) {
    print STDERR "Checking for file: $file\n";
    MiscUtils::test_exec($file);
  }
  print STDERR "...done.\n";
} ## end if ($doStage)
else {
  print
    "Skipping build, executable binaries already exist in ./$options->{rootName}\n";
}

print STDERR "Checking for files.\n";
MiscUtils::checkStageFiles(@{ $newFiles[$thisstage] })
  or die "Some stage $thisstage file is missing. Aborting!\n";

print "\n========== Generate Raw Table ==========\n";

MiscUtils::dataDiskFullCheckAndExit($options->{diskThreshold}, 1);
$thisstage = 2;
$doStage   = !MiscUtils::checkStageFiles(@{ $newFiles[$thisstage] });
if ($doStage) {
  # This is a parallel section, so pause here if pauseMode enabled
  if ($options->{pauseMode}) {
    open my $STDFOO, q{>&}, '3';
    select((select($STDFOO), $| = 1)[0])
      ;    # Force STDFOO to always flush buffer on write
    print $STDFOO
      "Pause Mode enabled. Press <Enter> or <Return> to continue with parallel gentable:";
    my $resp = <STDIN>;
    close $STDFOO;
  }

  my $stableoption = "";
  $stableoption = "-b" if ($options->{stableMode});
  my $cmd = "cd ./$options->{rootName}; " .
    "./parallel_gentable.pl -n $options->{cores} " .
    "$stableoption " .
    "-c $options->{prThreshold} " .
    "> rawtable-$options->{prThreshold} " .
    # Return parallel_gentable's errorlevel to system
    '&& exit 0 || c=$?; $(exit $c)';
  print "executing: $cmd \n";
  die "could not fork: $!" unless defined(my $GenRawTablePID = fork());

  if ($GenRawTablePID) {
    # Parent for GenRawTable process
    $prochandler->setAlive($GenRawTablePID);
  }
  else {
    # Child
    setpgrp $$, 0;  # Set this child to be the head of a new process group, so
                    # if it needs to be killed by the parent due to low disk
                    # space, it will kill all children
    my $returnstatus = system($cmd);
    if ($returnstatus != 0 && $returnstatus != 65280) {
      print STDERR "system $cmd returned errorcode: $returnstatus\n";
      die "exec of $cmd returned errorcode failed with: $!\n";
    }
    # If previous command returned successfully, use exec to kill this
    # child without running END block
    exec $^X => -eexit;      
  }

  my $start_run = time();

  #parent for GenRawTable process, will loop here until child is done, using SIGCHLD handler
  until ($prochandler->isDead($GenRawTablePID)) {
    $prochandler->checkChildErrorAndExit();
    if (MiscUtils::checkDataDiskFull($options->{diskThreshold}, 
                                     $options->{verbosity} > 2)) {
      print STDERR "\nAborting due to disk full!\n";
      print STDERR "Sending process $GenRawTablePID SIGTERM\n";
      kill -15, $GenRawTablePID;
      waitpid($GenRawTablePID, 0);
      print STDERR "Detected child process terminated.\n";
      exit($options->{exitcode});
    }
    print STDERR ".";    #status feedback while waiting
    if (time() - $options->{start_overall} > $options->{MAX_TIME}) {
      print STDERR "\nAborting due to time exceeded!\n";
      print STDERR "Reached maximum execution time "
        . MiscUtils::human_print_seconds($options->{MAX_TIME});
      print STDERR "Sending process $GenRawTablePID SIGTERM\n";
      kill -15, $GenRawTablePID;    # negative kills process group
      waitpid($GenRawTablePID, 0);
      print STDERR "Detected child process terminated.\n";
      exit($options->{exitcode});
    }
  }
  continue {
    sleep 1;
  }

  my $end_run  = time();
  my $run_time = $end_run - $start_run;
  print "\nGenerating raw table took $run_time seconds ("
    . MiscUtils::human_print_seconds($run_time) . ")\n";
} ## end if ($doStage)
else {
  print "Skipping Raw Table Generation\n";
}

MiscUtils::checkStageFiles(@{ $newFiles[$thisstage] })
  or die "Some stage $thisstage file is missing. Aborting!\n";
MiscUtils::dataDiskFullCheckAndExit($options->{diskThreshold}, 1);

print "\n========== Sort Raw Table ==========\n";

$thisstage = 3;
$doStage   = !MiscUtils::checkStageFiles(@{ $newFiles[$thisstage] });
if ($doStage) {
  # 1) Split raw table into small pieces where each piece can be sorted in RAM
  print "      ======splitting======    \n";
  my $start_run    = time();
  my $rawtablefile = @{ $newFiles[ $thisstage - 1 ] }[0];
  my $filesize = -s $rawtablefile;  # get size of file created in previous stage

  # If file is small enough, split so it uses all available cores, else make pieces small enough to fit in RAM
  my $splitsize = min floor(($filesize * 1.1) / $options->{cores}),
    floor($options->{memsliceh} * 1e9);

  MiscUtils::deleteFiles("$rawtablefile-split*", $options->{deleteMode});

  print "Raw table is $filesize bytes, " .
    "so splitting in $splitsize bytes chunks " .
    "to run on $options->{cores} cores\n";
  my $cmd = "split -a 6 -C $splitsize $rawtablefile $rawtablefile-split.";
  print "executing: $cmd \n";

  my $start_split = time();
  die "could not fork: $!" unless defined(my $splitPID = fork());
  if ($splitPID) {
    $prochandler->setAlive($splitPID);
  }
  else {
    # Child
    setpgrp $$, 0;
    my $returnstatus = system($cmd);
    if ($returnstatus != 0 && $returnstatus != 65280) {
      print STDERR "system $cmd returned errorcode: $returnstatus\n";
      die "exec of $cmd returned errorcode failed with: $!\n";
    }
    # If previous command returned successfully, use exec to kill this
    # child without running END block
    exec $^X => -eexit;      
  }
  until ($prochandler->isDead($splitPID)) {
    $prochandler->checkChildErrorAndExit();
    print STDERR "s";    #status feedback while waiting
    if (time() - $options->{start_overall} > $options->{MAX_TIME}) {
      kill -9, $splitPID;    #negative kills process group
      warn "\nreached maximum execution time "
        . MiscUtils::human_print_seconds($options->{MAX_TIME});
      exit($options->{exitcode});
    }
  }
  continue {
    MiscUtils::dataDiskFullCheckAndExit(
      $options->{diskThreshold},
      $options->{verbosity} > 2
    );
    sleep 1;
  }
  my $end_split  = time();
  my $split_time = $end_split - $start_split;
  print "\nRawtable split took $split_time seconds ("
    . MiscUtils::human_print_seconds($split_time) . ")\n";
  # Delete current rawtable since we have the splits
  MiscUtils::deleteFiles($rawtablefile, $options->{deleteMode});

  print "    =======sorting (<= $options->{cores} processes) =====\n";
  my $start_sort      = time();
  my $sortedtablefile = @{ $newFiles[$thisstage] }[0];
  # Delete any existing sorted split files
  MiscUtils::deleteFiles("$sortedtablefile*", $options->{deleteMode});
  my @rawsplitfiles = `find $options->{rootName} | grep "rawtable.*split.*\$"`;
  print "There are " . scalar(@rawsplitfiles) . " files to sort\n";

  # This is a parallel section, so pause here if pauseMode is enabled
  if ($options->{pauseMode}) {
    open my $STDFOO, q{>&}, '3';
    select((select($STDFOO), $| = 1)[0])
      ;    # Force STDFOO to always flush buffer on write
    print $STDFOO
      "Pause Mode enabled. Press <Enter> or <Return> to continue with parallel sort:";
    my $resp = <STDIN>;
    close $STDFOO;
  }

  my $mockcmd = "sort --parallel=1 -gr " .
    "-T ./$options->{rootName}/$options->{sortFolder}-??? " .
    "$rawtablefile-split.?? > $sortedtablefile-split.??";
  print "executing: '$mockcmd'\n";

  # Create a new process for each core
  for my $i (0 .. (scalar(@rawsplitfiles) - 1)) {
    # Due to memory requirements, we can start no more than $options->{cores} processes
    my $running = $prochandler->aliveCount();

    if ($running < $options->{cores}) {
      my $SortRawTablePID = fork();
      if ($SortRawTablePID) {
        # Parent process marks child as alive now
        #			$lock->down;
        $prochandler->setAlive($SortRawTablePID);
        #			$lock->up;
      }
      elsif ($SortRawTablePID == 0) {
        # Child process runs the sort
        $0 = "raw table sort process $i";
        setpgrp $$, 0;
        my $sortdir = "./$options->{rootName}/$options->{sortFolder}-$i";
        if (!(-d $sortdir)) {
          print STDERR "sort directory doesn't exist, creating\n"
            unless $options->{verbosity} < 3;
          my $cmd = "mkdir $sortdir";
          system($cmd) == 0 or die "system of '$cmd' failed with $?";
        }
        my $infile = $rawsplitfiles[$i];
        if (defined $infile) {
          chomp $infile;
          if (-e $infile) {
            my $cmd = "sort --parallel=1 -gr " .
              "-T $sortdir " .
              "$infile > $sortedtablefile-split.$i; " .
              "rm -rf $sortdir";
            print $cmd unless $options->{verbosity} < 2;
            print STDERR "child $i sorting\n";
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
            die "Raw table split file $infile doesn't exist and it should!\n";
          }
        }
        exit;
      } ## end elsif ($SortRawTablePID ==...)
      else {
        die "fork failed: $!\n";
      }
      $running++;
    } ## end if ($running < $options...)
    else {
      # We shouldn't be here -- we shouldn't have created this many processes, thanks to the below while loop...
      die "Too many processes ($running) were found in parallel sort!\n";
    }

    while ($running >= $options->{cores}) {
      # Don't fork until the number of running processes has dropped
      print "can't start sort child #"
        . ($i + 2) . " of "
        . (scalar(@rawsplitfiles))
        .
        " yet, already have $running children working " .
        "(" . (scalar(@rawsplitfiles) - ($i + 1)) . " waiting to start)\n"
        if (scalar(@rawsplitfiles) - ($i + 1)) > 0;
      sleep 60;

      $prochandler->checkChildErrorAndExit();
      MiscUtils::dataDiskFullCheckAndExit( 
        $options->{diskThreshold},
        $options->{verbosity} > 2
      );
      if (time() - $options->{start_overall} > $options->{MAX_TIME}) {
        # Time exceeded, kill all the children
        $prochandler->killAll();
        warn "\nreached maximum execution time "
          . MiscUtils::human_print_seconds($options->{MAX_TIME});
        exit($options->{exitcode});
      }
      $running = $prochandler->aliveCount();
    } ## end while ($running >= $options...)
  } ## end for my $i (0 .. scalar(...))

  print "\nWatching the remaining children work...\n";
  while ((my $running = $prochandler->aliveCount()) > 0) {
    $prochandler->checkChildErrorAndExit();
    MiscUtils::dataDiskFullCheckAndExit( 
      $options->{diskThreshold},
      $options->{verbosity} > 2
    );
    print "$running children still working\n";
    sleep 20;
    if (time() - $options->{start_overall} > $options->{MAX_TIME}) {
      #kill all the children
      $prochandler->killAll();
      warn "\nreached maximum execution time "
        . MiscUtils::human_print_seconds($options->{MAX_TIME});
      exit($options->{exitcode});
    }
  }

  my $end_sort  = time();
  my $sort_time = $end_sort - $start_sort;
  print "\nparallel sort took $sort_time seconds ("
    . MiscUtils::human_print_seconds($sort_time) . ")\n";
  MiscUtils::dataDiskFullCheckAndExit($options->{diskThreshold}, 1);

  # Delete rawtable pieces now that we have sorted splits
  MiscUtils::deleteFiles("$rawtablefile-split.*", $options->{deleteMode});

  # Need to make sure open file limit on system is increased for an increased batch size to work!
  $cmd = "sort -gr -m " .
    "-T $options->{rootName}/$options->{sortFolder} " .
    "--batch-size=$options->{ulimit} " .
    "$sortedtablefile-split* " .
    "> $sortedtablefile";
  print "executing: $cmd \n";
  my $start_merge = time();
  die "could not fork: $!" unless defined(my $mergePID = fork());

  if (!$mergePID) {
    # Child process
    if (!(-d "./$options->{rootName}/$options->{sortFolder}")) {
      print "sort directory doesn't exist, creating\n";
      my $cmd = "mkdir ./$options->{rootName}/$options->{sortFolder}";
      system($cmd) == 0 or warn "system of '$cmd' failed with $?";
    }
    setpgrp $$, 0;  # Set this child to be the head of a new process group, so
                # if it needs to be killed by the parent due to low disk
                # space, it will kill all children
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
    # Parent will wait until merge is done
    $prochandler->setAlive($mergePID);
    until ($prochandler->isDead($mergePID)) {
      $prochandler->checkChildErrorAndExit();
      print STDERR "m";    #status feedback while waiting
      if (time() - $options->{start_overall} > $options->{MAX_TIME}) {
        kill -9, $mergePID;    #negative kills process group
        warn "\nreached maximum execution time "
          . MiscUtils::human_print_seconds($options->{MAX_TIME});
        exit($options->{exitcode});
      }
    }
    continue {
      MiscUtils::dataDiskFullCheckAndExit(
        $options->{diskThreshold},
        $options->{verbosity} > 2
      );
      sleep 1;
    }
  } ## end else [ if (!$mergePID) ]

  my $end_merge  = time();
  my $merge_time = $end_merge - $start_merge;
  print "\nSorted table merge took $merge_time seconds ("
    . MiscUtils::human_print_seconds($merge_time) . ")\n";

  my $end_run  = time();
  my $run_time = $end_run - $start_run;
  print "\nTotal sort took $run_time seconds ("
    . MiscUtils::human_print_seconds($run_time) . ")\n";
} ## end if ($doStage)
else {
  print "Skipping Raw Table Sort \n";
}

MiscUtils::checkStageFiles(@{ $newFiles[$thisstage] })
  or die "Some stage $thisstage file is missing. Aborting!\n";
# This stage is over, delete previous stage files
foreach my $file (@{ $newFiles[ $thisstage - 1 ] }) {
  MiscUtils::deleteFiles("$file", $options->{deleteMode});
}
# Also delete the sorted split files now that we have a merged, sorted table
MiscUtils::deleteFiles(
  "@{$newFiles[$thisstage]}[0]-split*",
  $options->{deleteMode}
);

print "========== Aggregate Counts to Build Lookup Table ==========\n";

$thisstage = 4;
$doStage   = !MiscUtils::checkStageFiles(@{ $newFiles[$thisstage] });
if ($doStage) {
  my $cmd = "./$options->{rootName}/sortedcountaggregator " .
    "< @{$newFiles[$thisstage-1]}[0] " .
    "> @{$newFiles[$thisstage]}[0]";
  print "executing: $cmd \n";
  die "could not fork(): $!" unless defined(my $LookupTablePID = fork());
  if ($LookupTablePID) {
    $prochandler->setAlive($LookupTablePID);
  }
  else {
    # Child
    setpgrp $$, 0;  # Set this child to be the head of a new process group, so
                    # if it needs to be killed by the parent due to low disk
                    # space, it will kill all children
    my $returnstatus = system($cmd);
    if ($returnstatus != 0 && $returnstatus != 65280) {
      print STDERR "system $cmd returned errorcode: $returnstatus\n";
      die "exec of $cmd returned errorcode failed with: $!\n";
    }
    # If previous command returned successfully, use exec to kill this
    # child without running END block
    exec $^X => -eexit;      
  }
  my $start_run = time();
  until ($prochandler->isDead($LookupTablePID)) {
    $prochandler->checkChildErrorAndExit();
    print STDERR "l";    #status feedback while waiting
    if (time() - $options->{start_overall} > $options->{MAX_TIME}) {
      kill -9, $LookupTablePID;    #negative kills process group
      warn "\nreached maximum execution time "
        . MiscUtils::human_print_seconds($options->{MAX_TIME});
      exit($options->{exitcode});
    }
  }
  continue {
    MiscUtils::dataDiskFullCheckAndExit(
      $options->{diskThreshold},
      $options->{verbosity} > 2
    );
    sleep 1;
  }

  my $end_run  = time();
  my $run_time = $end_run - $start_run;
  print "\nBuilding lookup table took $run_time seconds ("
    . MiscUtils::human_print_seconds($run_time) . ")\n";
} ## end if ($doStage)
else {
  print "Skipping Lookup Table Aggregation\n";
}

MiscUtils::checkStageFiles(@{ $newFiles[$thisstage] })
  or die "Some stage $thisstage file is missing. Aborting!\n";
# This stage is over, delete previous stage files
# This should *not* delete the lookup table.  If desired, that will be
#   deleted by iterate_experiments when the condition directory is deleted.
foreach my $file (@{ $newFiles[ $thisstage - 1 ] }) {
  MiscUtils::deleteFiles("$file", $options->{deleteMode});
}


my $end_overall = time();
my $run_overall = $end_overall - $options->{start_overall};
print "\n======================================\n" .
  "Done! Entire run took $run_overall seconds ("
  . MiscUtils::human_print_seconds($run_overall) . ")\n"
  .
  "======================================\n";

# Kill processes, flush I/O, and then exit -- I haven't found any other way to
# ensure that a 0 is returned.
$prochandler->killAll();
$| = 1;
_exit(0);
