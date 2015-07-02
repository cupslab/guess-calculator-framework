# Miscellaneous utility functions for the guess calculator framework
#
# Almost all of these utilities die on error, since errors can produce
#   inaccurate results.
#
# Written by Saranga Komanduri and Michelle Mazurek
package MiscUtils;

use strict;
use Exporter;
use Safe;

our @ISA     = qw(Exporter);
our $VERSION = 1.10;
our @EXPORT_OK =
  qw(debug_print run_cmd test_exist test_exec next_rand_pick read_cfg count_lines);

our $DEBUG = 1;

sub debug_print {
  # Print the given string, prepended with a timestamp, if DEBUG is set
  my $arg = shift @_;
  if ($DEBUG) {
    my $t = localtime;
    print STDERR "$t - $arg";
  }
}

sub run_cmd {
  # Simple function for running a command, checking the error level, and outputting the error and quitting
  # The first argument is the command to run IN QUOTES
  # Any extra message to be displayed on error can be added as a second argument
  # Returns the stdout of the command that is run to maintain compatibility with perl backticks
  my ($RUNCMD, $MSG) = @_;

  if ($RUNCMD eq '')
  {
    print "No arguments passed to run_cmd! Arguments: @_\n";
    exit 1;
  }

  # Run the command and store output
  if ($DEBUG) {
    my $t = localtime;
    print STDERR "$t - Running cmd: $RUNCMD\n";
  }
  my @cmdoutput = `$RUNCMD`;

  # Check errorlevel
  if ($? != 0)
  {
    if (!$MSG) {
      $MSG = "";
    }
    print "Command \"$RUNCMD\" returned a nonzero errorlevel! $MSG\n";
    exit 1;
  }

  return (@cmdoutput);
} ## end sub run_cmd

sub test_exist {
  # Test if argument file exists
  my $file = shift;
  die "FATAL: couldn't locate required file \"$file\"\n" if (!-e $file);
}

sub test_exec {
  # Test if argument file is executable and make executable if it is not
  my $file = shift;
  test_exist($file);
  if (!-x $file) {
    system("chmod u+x $file") == 0
      or die "couldn't set exec bin on \"$file\": $?\n";
  }
}

sub read_cfg {
  # Read a configuration file into a namespace (CFG is the default)
  # die if there's a problem, only return on success
  my $file = shift;
  my $namespace = shift;
  $namespace = "CFG" if !(defined $namespace);

  if (-e "$file") {
    # Put config data into a separate namespace
    my $compartment = new Safe $namespace;

    # Process the contents of the config file
    my $result = $compartment->rdo($file);

    # Check for errors
    if ($@) {
      die "ERROR: Failure compiling '$file' - $@";
    }
    elsif (!defined($result)) {
      die "ERROR: Failure reading '$file' - $!";
    }
    elsif (!$result) {
      die "ERROR: Failure processing '$file'";
    }
  }
  else {
    die "ERROR: Config file $file does not exist!";
  }
  return;
} ## end sub read_cfg


sub valid_condition_name {
  # Test if name is a valid filename and doesn't begin with tokens used in temporary files
  # Returns boolean
  my $name = shift;
  my $result = 1;

  # Check if condition name is compliant
  if ($name !~ /^[\w.-]+$/) {
    warn "Condition name: $name not a valid filename\n";
    $result = 0;
  }
  if ( $name =~ /^(combined|calculator|structuretraining|dandstraining|stringtraining)/) {
    warn "Condition name: $name must not begin with a reserved word!\n";
    $result = 0;
  }

  return $result;
}


sub test_condition_cfg {
  # For conditions used as input to iterate_experiments,
  #   test if the files they reference exist
  # Return false (0) on fail, true (1) on pass
  my @conditionfiles = @_;
  my $shouldExit     = 0;
  foreach my $file (@conditionfiles) {
    if (-e "$file") {
      if (my $err = MiscUtils::read_cfg($file)) {
        print(STDERR $err, "\n");
        warn "Config file format error in $file\n";
        $shouldExit = 1;
      }
    }
    else {
      warn "Couldn't open config file: $file\n";
      $shouldExit = 1;
    }

    my @conditionfiles = (qw/structures terminals lookup/);
    foreach my $cfile (@conditionfiles) {
      if (!-e $CFG::CFG{$cfile}) {
        warn "Couldn't open $cfile file: $CFG::CFG{$cfile} in $file\n";
        $shouldExit = 1;
      }
    }

    # Check if condition name is compliant
    if (!valid_condition_name($CFG::CFG{'name'})) {
      $shouldExit = 1;      
    }
  } ## end foreach my $file (@conditionfiles)

  if ($shouldExit) {
    return 0;
  }

  return 1;
} ## end sub test_condition_cfg

sub format_cutoff {
  my $cut = shift;
  if ($cut =~ /^\d+$/) {
    $cut = '1e-' . $cut;
  }
  return $cut;
}

sub test_condition_cfg_post_dictionary {
  # After dictionary is built, check that expected files exist
  # Return false (0) on fail, true (1) on pass
  my $conditionfile = shift;
  my $foundError    = 0;

  if (-e "$conditionfile") {
    if (my $err = MiscUtils::read_cfg($conditionfile)) {
      print(STDERR $err, "\n");
      warn "config file format error in $conditionfile\n";
      $foundError = 1;
    }
  }
  else {
    warn "couldn't open config file: $conditionfile\n";
    $foundError = 1;
  }
  # Check cutoff formatting
  if ($CFG::CFG{'cutoffmin'} !~ /^\d+$|\de-/) {
    warn "cutoff $CFG::CFG{'cutoffmin'} not formatted correctly in $conditionfile\n";
    $foundError = 1;
  }
  # Check for test set file
  if (!-e $CFG::CFG{'lookup'}) {
    warn "couldn't open lookup file: $CFG::CFG{'lookup'} in $conditionfile\n";
    $foundError = 1;
  }
  # Construct expected calculator zip file name and check
  my $name = $CFG::CFG{'name'};
  my $cut  = format_cutoff($CFG::CFG{'cutoffmin'});
  if (!-e "$name-$cut/calculator.zip") {
    warn
      "couldn't find calculator.zip file: $name-$cut/calculator.zip relating to $conditionfile\n";
    $foundError = 1;
  }

  if ($foundError) {
    return 0;
  }

  return 1;
} ## end sub test_condition_cfg_post_dictionary

sub count_lines {
  # Count lines in a file using wc
  my $file = shift;
  my @out  = run_cmd "wc -l \"$file\"";
  $out[0] =~ /^\s*(\d+)\s+/;
  return $1;
}

sub deleteFiles {
  # Delete files (including potential wildcards) or log files that would be deleted
  # If deleteMode is true (1), delete files, else remove them
  my $path       = shift;
  my $deleteMode = shift;
  if ($deleteMode) {
    my $cmd = "rm -rfv $path 1>&2";
    print STDERR "ATTEMPTING DELETE: $cmd\n";
    system($cmd);
  }
  else {
    my $cmd = "ls $path 1>&2";
    print STDERR "WOULD HAVE DELETED: $cmd\n";
    system($cmd);
  }
}

sub human_print_seconds {
  # Print the amount of time in argument with appropriate time unit
  my $s    = shift;
  my $out  = $s;
  my $unit = "seconds";
  if ($s > 120) {    #two min
    $unit = "minutes";
    $out /= 60;
  }
  if ($s > 120 * 60) {    #two hours
    $unit = "hours";
    $out /= 60;
  }
  if ($s > 48 * 60 * 60) {    #two days
    $unit = "days";
    $out /= 24;
  }
  return "$out $unit";
}

sub dataDiskFullCheckAndExit {
  # Check if available disk space is lower than threshold and exit if so
  # Print debug information if second argument is true
  my $threshold = shift;
  my $verbose   = shift;
  if (checkDataDiskFull($threshold, $verbose)) {
    print STDERR "Aborting!!!\n";
    exit(1);
  }
}

sub checkDataDiskFull {
  # Return 1 if current disk is above threshold
  my $threshold = shift;
  my $verbose   = shift;
  my $rval      = 0;
  my $df        = dataDiskPercentUsed($verbose);
  print STDERR "disk is at $df percent\n" if $verbose;
  if ($df > $threshold) {
    print STDERR "exceeded disk threshold ($threshold)!!!\n";
    $rval = 1;
  }
  return $rval;
}

sub dataDiskPercentUsed {
  # Returns used percent of current disk
  # Returns 100% full on error
  my $verbose = shift;
  my $rval    = 100;
  my $df      = `df .`;
  #Filesystem      1K-blocks      Used  Available Use% Mounted on
  #/dev/sdb1      8784708908 376873428 7968406720   5% /data
  if ($df =~ /\/dev[^\s]+\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)/) {
    my $tot     = $1;
    my $used    = $2;
    my $avail   = $3;
    my $percent = $4;
    print STDERR
      "Disk usage: Total: $tot  Used: $used  Available: $avail  Used %: $percent\n"
      if $verbose;

    $rval = $percent;
  }

  return $rval;
} ## end sub dataDiskPercentUsed

# Following sub copied verbatim from:
# http://www.use-strict.de/perl-file-size-in-a-human-readable-format.html
sub get_filesize_str
{
    my $file = shift();

    my $size = (stat($file))[7] || die "stat($file): $!\n";

    if ($size > 1099511627776)  #   TiB: 1024 GiB
    {
        return sprintf("%.2f TiB", $size / 1099511627776);
    }
    elsif ($size > 1073741824)  #   GiB: 1024 MiB
    {
        return sprintf("%.2f GiB", $size / 1073741824);
    }
    elsif ($size > 1048576)     #   MiB: 1024 KiB
    {
        return sprintf("%.2f MiB", $size / 1048576);
    }
    elsif ($size > 1024)        #   KiB: 1024 B
    {
        return sprintf("%.2f KiB", $size / 1024);
    }
    else                        #   bytes
    {
        return "$size byte" . ($size == 1 ? "" : "s");
    }
}


sub checkStageFiles {
  # Return 1 if files at this stage exist and output to log
  my @newFiles = @_;
  my $rval     = 0;
  foreach my $file (@newFiles) {
    if (-e "./$file" && -s "./$file") {
      my $filesize = get_filesize_str("./$file");
      print STDERR "\nfile $file found and is $filesize in size!\n";
      $rval = 1;
    }
    else {
      print STDERR "file $file not found...\n";
      $rval = 0;
      last;
    }
  }

  return $rval;
}

1;
