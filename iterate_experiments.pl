#!/usr/bin/perl
###############################################################################
# iterate_experiments.pl
# This script processes conditions through the guess calculator framework based
#   on a specified configuration file.  A configuration file can hold multiple
#   conditions.
#
# Written by Saranga Komanduri and Tim Vidas
# Formatted by perltidy
use strict;
use warnings;

# Load Perl modules
use Cwd 'abs_path';

# Load local modules
use IterateExperimentsOptionsHandler;
use lib 'binaries';
use MiscUtils;

my $VERSION = "0.82";                    # Thu Apr 10 11:41:39 2014

#####
##### Initialization
#####

# Process command-line options
my $options = IterateExperimentsOptionsHandler->new({ VERSION => $VERSION });
$options->process_command_line();
$options->set_cores();

# Check that all files referenced in configuration files exist
if (!MiscUtils::test_condition_cfg(@{ $options->{conditions} })) {
  die "Configs errors need to be corrected\n";
}

#####
##### Subroutines
#####

sub deleteFiles {
  my $files = shift;
  MiscUtils::deleteFiles($files, $options->{deleteMode});
}

sub makeTables {
  # Call single_run.pl and put result in $name-$cut directory

  # Process input parameters into arguments for single_run
  my $name          = $_[0];
  my $structurefile = $_[1];
  my $terminalsfile = $_[2];
  my $cutoff        = $_[3];
  my $option        = "";
  if (defined($_[4])) {
    $option .= $_[4];
  }
  my $tquantoption = "";
  if (defined($_[5])) {
    $tquantoption = "-T $_[5]";
  }
  my $iquantoption = "";
  if (defined($_[6])) {
    $iquantoption = "-I $_[6]";
  }
  my $gunseenoption = "";
  if ($_[7]) {
    $gunseenoption = "-g";
  }  
  my $deloption = "";
  if (!$options->{deleteMode}) {
    $deloption = "-d";
  }
  my $pauseoption = "";
  if ($options->{pauseMode}) {
    $pauseoption = "-p";
  }
  my $stableoption = "";
  if ($options->{stableMode}) {
    $stableoption = "-b";
  }

  my $logfile = "$name-$cutoff.log";
  system("echo 'starting $name - $structurefile $terminalsfile $cutoff' >> \"$logfile\"") == 0
    or die "makeTables echo failed";
  system("date >> \"$logfile\"") == 0 or die "makeTables datestamp failed";
  print "Starting iteration: ";
  my $cmd = "perl single_run.pl $option " .
    "-n $options->{cores} " .
    "-s \"$structurefile\" " .
    "-c \"$terminalsfile\" " .
    "-C $cutoff " .
    "$deloption $pauseoption $gunseenoption $tquantoption $iquantoption $stableoption " .
    ">> \"$logfile\" 2>&1; c=\$\?; echo \$c; exit \$c";
  print "executing: $cmd \n";
  my $single_run_ret = system($cmd);
  print STDERR "single_run.pl returned exitcode: ${single_run_ret}\n";

  if ($single_run_ret != 0)
  {    
    # This means that something bad happened
    warn
      "Single run did not exit successfully!  Check $logfile for further details.\n";
    return 1;
  }
  system("date >> \"$logfile\"") == 0 or die "makeTables datestamp failed";
  system("mkdir $name-$cutoff") if (!(-d "$name-$cutoff"));
  if ($option eq "-o") {
    system("mv $options->{rootName}* ./$name-$cutoff") == 0
      or die "makeTables PCFG move failed";
  }
  else {
    # Move the whole directory with lookup table
    system("mv $options->{rootName} ./$name-$cutoff") == 0
      or die "makeTables directory move failed";
  }

  return 0;
} ## end sub makeTables

sub doLookups {
  # Lookup test set passwords in lookup table using map_lookup, parallel_lookup, and reduce_lookup
  # map_lookup can produce multiple patterns to lookup for each password,
  #   then the reduce step takes the min guess number for each password's patterns
  # Existence of scripts and PatternLookup was tested in single_run
  my $name   = $_[0];
  my $lookup = $_[1];
  my $cutoff = $_[2];

  my $conditionfile = "$name-$cutoff";

  # print "Mapping each test set password to all matching structures\n";
  # my $cmd = "cd ./$conditionfile/$options->{rootName}/ " .
  #   "&& ruby map_lookup.rb \"$lookup\" > expanded_lookup.txt";
  # print "executing: $cmd\n";
  # system($cmd) == 0 or die("map_lookup failed!");

  # Pause before calling parallel_lookup, if pauseMode is enabled
  if ($options->{pauseMode}) {
    open my $STDFOO, q{>&}, '3';
    select((select($STDFOO), $| = 1)[0])
      ;    # Force STDFOO to always flush buffer on write
    print $STDFOO
      "Pause Mode enabled. Press <Enter> or <Return> to continue with parallel lookup (WARNING THIS WILL USE $options->{cores} CORES):";
    my $resp = <STDIN>;
    close $STDFOO;
  }

  print "Looking up all matching passwords\n";
  my $cmd = "cd ./$conditionfile/$options->{rootName}/ " .
    "&& ./parallel_lookup.pl " .
    "-n $options->{cores} " .
    "-L lookuptable-$cutoff " .
    "-I \"$lookup\" " .
    "> ../../lookupresults.$conditionfile; ".
    "c=\$\?; echo \$c; exit \$c";
  print("  executing: $cmd\n");
  my $lookup_ret = system($cmd);
  if ($lookup_ret != 0) {
    die("PatternLookup failed with exit code: $lookup_ret!\n");
  }

  # print "Taking lowest guess number for each test set password\n";
  # $cmd = "cd ./$conditionfile/$options->{rootName}/ " .
  #   "&& ruby reduce_lookup.rb \"$lookup\" expanded_lookup_results.txt " .
  #   "> ../../lookupresults.$conditionfile";
  # print("  executing: $cmd\n");
  # system($cmd) == 0 or die("reduce_lookup failed!");
} ## end sub doLookups

#####
##### Iterate over conditions
#####
foreach my $file (@{ $options->{conditions} }) {
  # This was already tested so we should not fail here, but check anyway
  if (my $err = MiscUtils::read_cfg($file)) {
    print(STDERR $err, "\n");
    exit(1);
  }

  # Search for the calculator zip in the condition directory
  if (!$options->{onlyMakeDictionaries}) {
    # Check if PCFG specification exists
    if (
      !MiscUtils::test_condition_cfg_post_dictionary($file))
    {
      warn "PCFG specification not found.  Will rebuild.\n";
      $options->{mustRebuild} = 1;
    }
  }


  print "Processing " . $CFG::CFG{'name'} . ": " . $CFG::CFG{'desc'} . "\n";

  # Load config values into local variables
  my $mincutoffstr = MiscUtils::format_cutoff($CFG::CFG{'cutoffmin'});
  my $maxcutoffstr = MiscUtils::format_cutoff($CFG::CFG{'cutoffmax'});
  my $generate_unseen_terminals = 0;                                   # default
  my $tquantlevels = 500;                                              # default
  my $iterquant    = 1000;                                             # default
  if ($CFG::CFG{'generate_unseen_terminals'}) {
    $generate_unseen_terminals = 1;
  }
  if ($CFG::CFG{'totalquantlevels'} =~ /^\d+$/) {
    $tquantlevels = $CFG::CFG{'totalquantlevels'};
  }
  if ($CFG::CFG{'quantizeriters'} =~ /^\d+$/) {
    $iterquant = $CFG::CFG{'quantizeriters'};
  }



  # Delete files that might be leftover from a cancelled run
  deleteFiles("dandstraining*");
  deleteFiles("structuretraining*");
  deleteFiles("stringtraining*");
  deleteFiles("combined*");
  deleteFiles("$options->{rootName}");
  # Delete other files if necessary
  if ($options->{onlyMakeDictionaries} || $options->{mustRebuild}) {
    # Remove previous directories, but dont remove the logfiles
    my $conditionstub = $CFG::CFG{'name'} . '-.e';
    my @conditiondirs = `find . -maxdepth 1 -type d | egrep "^$conditionstub"`;
    print "Deleting any previous condition files\n";
    foreach my $dir (@conditiondirs) {
      deleteFiles($dir);
    }
    # Remove any leftover calculator files
    deleteFiles("$options->{rootName}*");
  }

  if ($options->{onlyMakeDictionaries}) {
    print "Only building the PCFG for this condition\n";

    # Build the PCFG using -o option to single_run
    my $cutoffstr = $mincutoffstr;
    if (
      makeTables(
        $CFG::CFG{'name'},
        $CFG::CFG{'structures'},
        $CFG::CFG{'terminals'},
        $cutoffstr,
        "-o",
        $tquantlevels,
        $iterquant,
        $generate_unseen_terminals
      ) == 0
      )
    {
      print "$options->{rootName}.zip created.\n";
    }
    else {
      print "makeTables on "
        . $CFG::CFG{'name'}
        . " returned non 0 exit code. Check condition specific logfiles for more information.\n";
    }
  } ## end if ($options->{onlyMakeDictionaries...})
  else {
    # Build lookup tables using single_run and then lookup test set passwords
    print
      "Building guess calculator for probability thresholds $mincutoffstr through $maxcutoffstr (or disk exhaustion)\n";

    my $endIteration =
      0;    # Flag indicating that an experiment has failed so stop iterating

    # Iterate probability thresholds through powers of 10
    for (
      my $cutoff = $mincutoffstr ;
      $endIteration == 0 && $cutoff >= $maxcutoffstr ;
      $cutoff = $cutoff / 10
      )
    {
      my $cutoffstr = "$cutoff";

      # If we already have a generated zip calculator, use that
      my $firstdir = "$CFG::CFG{'name'}-$mincutoffstr";
      if (-e "./$firstdir/$options->{rootName}.zip") {
        system("cp ./$firstdir/$options->{rootName}.zip ./") == 0
          or die "PCFG zip copy failed!\n";
      }
      else {
        print "PCFG archive zip file not found in $firstdir, no copy made\n";
      }

      # Build the lookup table by calling single_run without the -o option
      print "Building for $CFG::CFG{'name'}, $cutoffstr\n";
      if (
        makeTables(
          $CFG::CFG{'name'},
          $CFG::CFG{'structures'},
          $CFG::CFG{'terminals'},
          $cutoffstr,
          "",
          $tquantlevels,
          $iterquant,
          $generate_unseen_terminals
        ) == 0
        )
      {

        print "Tables built successfully, performing lookups\n";
        my $lookupfile = abs_path($CFG::CFG{'lookup'});
        doLookups($CFG::CFG{'name'}, $lookupfile, $cutoffstr)
          ;    # doLookups dies on any error, so if we make it here it worked

        print "Finished lookups " . $CFG::CFG{'name'} . "\n";
        my $lookupoutput =
          "lookupresults." . $CFG::CFG{'name'} . "-" . $cutoffstr;
        print "Archiving Total count for " . $CFG::CFG{'name'} . "\n";
        my $lookuptable = "./"
          . $CFG::CFG{'name'} . "-"
          . "$cutoffstr/$options->{rootName}/lookuptable-$cutoffstr";
        # The last line of the lookup table has the total count of guesses covered by the table
        if (
          system(
            "tail $lookuptable | "
              .
              "fgrep -H 'Total count' " .
              "> totalcounts." . $CFG::CFG{'name'} . "-" . $cutoffstr
          ) != 0
          )
        {
          # If the single_run was killed (time/disk expired), then Total count won't exist,
          #   so don't continue with next iteration
          print "Error: could not determine Total count in $lookuptable!!\n";
          $endIteration = 1;
        }
        if ($endIteration == 0) {
          # Check that we have as many guess numbers as input passwords
          # TODO: It would be better to check that the passwords in the lookup file
          #   match the passwords in the lookup output.
          print "Comparing line counts of lookup output to test set file\n";
          if (
            MiscUtils::count_lines($lookupoutput) !=
            MiscUtils::count_lines($lookupfile)
            )
          {
            print "Error: line counts don't match!\n";
            $endIteration = 1;
          }
          else {
            print "..line counts match!\n";
          }
        } ## end if ($endIteration == 0)

        # Cleanup
        if ($options->{deleteConditionDirectory}) {
          deleteFiles("$CFG::CFG{'name'}-$cutoffstr");
        }

      } ## end if ( makeTables( $CFG::CFG...))
      else {
        die "makeTables on "
          . $CFG::CFG{'name'}
          . " returned non 0 exit code. Check condition specific logfiles for more information.\n";
      }
    } ## end for ( my $cutoff = $mincutoffstr...)
        # On final cleanup, we want to remove the zip file for the condition
        #   unless it was just built
    if ($options->{mustRebuild}) {
      if ($options->{deleteConditionDirectory}) {
        # If the condition directory is deleted we can't move the zip there, save it separately
        my $pcfgzip = "PCFG-$CFG::CFG{'name'}.zip";
        if (-e $pcfgzip) {
          warn "PCFG zip already exists at $pcfgzip\n"
        }
        else {
          system("mv $options->{rootName}.zip $pcfgzip") == 0
            or die "makeTables PCFG rename failed";
        }
      }
      else {
        # Else move it to the first condition directory
        my $firstdir = "$CFG::CFG{'name'}-$mincutoffstr";
        system("mv ./$options->{rootName}.zip ./$firstdir") == 0
          or die "makeTables PCFG move failed";
      }
    } ## end if ($options->{mustRebuild...})
    deleteFiles("$options->{rootName}*");
  } ## end else [ if ($options->{onlyMakeDictionaries...})]

  print "Completed processing config file $file\n";  
} ## end foreach my $file (@{ $options...})

print "Completed processing all config files!\n";
exit 0;