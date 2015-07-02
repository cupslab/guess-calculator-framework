# Object to hold configuration options for single_run.pl and process command-line options
#
# Written by Saranga Komanduri
package SingleRunOptionsHandler;

use strict;
use warnings;
use Getopt::Std;
use lib 'binaries';
use MiscUtils;

sub new {
  # Constructor
  my ($class, $args) = @_;
  my $self = {
    # Options which control execution
    BLOCAL               => 1,         # set to 0 if running on passbox
    MAX_TIME             => 3456000,   #max run time for single run (in seconds)
                                       #3456000 is 40 days (no cutoff)
    ulimit               => 16,
    cores                => 1,
    memh                 => 4,
    memsliceh            => 1.6,
    diskThreshold        => 95,        #percent when disk is considered full
    quantLevels          => 500,
    quantIters           => 1000,
    prThreshold          => "1e-12",
    coresSet             => 0,
    onlyMakeDictionaries => 0,
    deleteMode           => 1,         #default to deleting files
    pauseMode            => 0,
    stableMode           => 1,
    generateUnseen       => 0,

    # File / folder paths
    rootName      => 'calculator',
    sortFolder    => 'sortdir',
    structureFile => '',               # required input
    terminalsFile => '',               # required input

    # Miscellaneous variables referenced in single_run
    start_overall => time(),
    exitcode      => 5
    , # indicates MAX_TIME runtime exceeded, must match expected exit code in iterate_experiments
    verbosity => 0,
    version   => $args->{VERSION}
  };
  return bless $self, $class;
} ## end sub new

sub print_usage {
  # Print usage in case of invalid command-line arguments
  my ($self, $arg) = @_;
  print STDERR "missing $arg \n" if defined $arg;

  print STDERR << "EOF";
  Single-condition automation script v$self->{version}

  $0 [-hodgbp][-n ?][-C #][-v #][-T #][-I #]  -s file1 -c file2

  this script requires several arguments:
    -s input_structures_file  structures to be learned / generate guesses for
    -c input_terminals_file   terminals to be learned / generate guesses for

  there are also several optional arguments:
    -h   this help
    -v # set verbosity level 1(lowest) through 5  (currently not implemented)
    -n # set the number of cores to utilize 
         (defaults to (available - 2), as reported by /proc/cpuinfo; or 1)
    -C # set probability threshold where # is of the form 1e-15 (defaults to 1e-12)
    -D   set sort directory (default is "sortdir" in the current directory)
    -R   set root name for calculation directory (default is "calculator")
    -o   only create dictionary.zip (stage1), then stop    
    -d   if specified the script will automatically delete files as it
         progresses through each experiment stage
    -g   include patterns for unseen terminals in the PCFG specification
    -b   random mode (random operations DO NOT use a set seed for deterministic results)
    -p   pause for input from STDIN before starting parallel sort
    -T # set the total number of levels for the quantizer
    -I # set the total number of iterations for the quantizer

  example:
    $0 -s S1_structures.gz -c S1_terminals.gz

EOF

  exit 1;
} ## end sub print_usage

sub process_command_line {
  # Process command line options and set config variables in object hash appropriately
  my $self = shift;

  my %opts;
  getopts('c:s:hdobgpn:v:C:D:T:I:R:', \%opts);

  # On -h just print usage and quit
  $self->print_usage() if defined $opts{h};

  # Check that required file arguments are specified and then check that they exist
  # Map command-line option to getopts ID to config variable key
  my @reqArguments = (
    [qw/-s s structureFile/],
    [qw/-c c terminalsFile/]
  );
  foreach my $reqarg (@reqArguments) {
    $self->print_usage(@$reqarg[0]) if !defined $opts{ @$reqarg[1] };
    $self->{ @$reqarg[2] } = $opts{ @$reqarg[1] };
    MiscUtils::test_exist($self->{ @$reqarg[2] });
  }

  # Set config variables for those options that are specified (default values were set in the constructor)
  # Map command-line option to config variable key
  my @optArguments = (
    [qw/T quantLevels ^\\d+$/],
    [qw/I quantIters ^\\d+$/],
    [qw/C prThreshold ^[\\d.]+e-\\d+$/]
    ,    # probability threshold is allowed to be in 1e-12 notation
    [qw/D sortFolder ^[\\w.-]+$/],
    [qw/R rootName ^[\\w.-]+$/],
    [qw/v verbosity ^\\d+$/],
    [qw/n cores ^\\d+$/]
  );
  foreach my $optarg (@optArguments) {
    if (defined $opts{ @$optarg[0] }) {
      if ($opts{ @$optarg[0] } =~ /@$optarg[2]/) {
        $self->{ @$optarg[1] } = $opts{ @$optarg[0] };
      }
      else {
        die
          "Option @$optarg[0] was malformed!  Read in as $opts{@$optarg[0]}\n";
      }
    }
  }

  # Set config variables for those options which have no associated value or when we want to store that a value was specified (as in number of cores)
  my @optBooleans = (
    [qw/o onlyMakeDictionaries 1/],
    [qw/d deleteMode 0/],
    [qw/p pauseMode 1/],
    [qw/n coresSet 1/],
    [qw/b stableMode 0/],
    [qw/g generateUnseen 1/]
  );
  foreach my $optbool (@optBooleans) {
    $self->{ @$optbool[1] } = @$optbool[2] if defined $opts{ @$optbool[0] };
  }
} ## end sub process_command_line

sub set_cores {
  # Set the number of cores to use based on the amount of RAM available and number of cores available, if on the Andrew machine
  # Also set the sort slice size {memsliceh}
  my $self = shift;

  # If cores were set externally, use available RAM to determine sort slice size
  if ($self->{coresSet}) {
    my $ramreserve = 5000 *
      1024;    # reserve an extra 5 GB of ram for any system overhead
    chomp(
      my $mem1 =
        `cat /proc/meminfo | grep ^MemFree| grep -o \'[0-9]\\{1,\\}\'`
    );
    if (!$mem1 =~ /^\d+$/) {
      die "odd, MemFree in /proc/meminfo is not a number: $mem1 \n";
    }
    chomp(
      my $mem2 =
        `cat /proc/meminfo | grep ^Cached| grep -o \'[0-9]\\{1,\\}\'`
    );
    if (!$mem2 =~ /^\d+$/) {
      die "odd, Cached in /proc/meminfo is not a number: $mem2 \n";
    }
    my $mem = $mem1 + $mem2;
    print STDERR "found "
      . $mem1
      . "kb of free RAM and "
      . $mem2
      . "kb of cache for "
      . $mem
      . "kb total\n";
    $mem -= $ramreserve;       #subtract out the ramdisk
    if ($mem < 0) {
      $mem = 2000 * 1024;
    }
    print STDERR "using " . $mem . "kb of RAM after reserving some for system.";

    # Compute sort slice size
    $self->{memh} = $mem / 1024 / 1024;    # memory amount in GB
    my $memslice = $mem / $self->{cores} / 2 / 1.25
      ; # amount per thread, divide by 2 because sort requires 2n space for sorting, then divide by 1.25 for 25% slack
    $self->{memsliceh} = $memslice / 1024 / 1024;   # memslice in GB
  }

  # TODO: Remove BLOCAL switch once we have verified that the code works without it
  if ($self->{BLOCAL} == 0) {
    # Specific options used for the passbox setup in the Andrew project (passwords were stored on a ramdisk so they would never touch the disk)

    if (!$self->{coresSet}) {
      chomp(my $cores = `cat /proc/cpuinfo | grep processor | wc -l`);
      if (!$cores =~ /^\d+$/) {
        print STDERR
          "odd, cpuinfo shows this for cores: $cores\n";    # don't change cores
      }
      else {
        if ($cores > 3) {
          $self->{cores} = $cores -
            2;    # reserve two cores so that the management thread gets cputime
        }
      }
    }

    chomp(
      my $ramdisk =
        `df  | grep "/data/ramdisk" | grep -o 'tmpfs[[:space:]]\\{1,\\}[0-9]\\{1,\\}'| grep -o '[0-9]\\{1,\\}'`
    );
    if (!$ramdisk =~ /^\d+$/) {
      die "couldn't find ramdisk size at /data/ramdisk";
    }
    else {
      print STDERR "found " . $ramdisk / 1024 . " MB ramdisk\n";
    }

    my $ramreserve = 5000 *
      1024;    # reserve an extra 5 GB of ram for any system overhead
    chomp(
      my $mem =
        `cat /proc/meminfo | grep MemTotal | grep -o \'[0-9]\\{1,\\}\'`
    );
    # If really want free RAM, use the lines below
    if ($self->{pauseMode}) {
      chomp(
        my $mem1 =
          `cat /proc/meminfo | grep ^MemFree| grep -o \'[0-9]\\{1,\\}\'`
      );
      chomp(
        my $mem2 =
          `cat /proc/meminfo | grep ^Cached| grep -o \'[0-9]\\{1,\\}\'`
      );
      $mem = $mem1 + $mem2;
      print STDERR "found "
        . $mem1
        . "kb of free RAM and "
        . $mem2
        . "kb of cache for "
        . $mem
        . "kb total\n";
    }
    if (!$mem =~ /^\d+$/) {
      print STDERR "odd, meminfo grep shows this for mem: $mem \n";
      $mem = 524288;    #default to 512
    }
    else {
      $mem -= $ramdisk;       #subtract out the ramdisk
      $mem -= $ramreserve;    #subtract out the reserve amount
    }

    $self->{memh} = $mem / 1024 / 1024;    # human readable memory amount
    my $memslice = $mem / $self->{cores} / 2 / 1.25
      ; # amount per thread, divide by 2 because sort requires 2n space for sorting (with 25% slack)
    $self->{memsliceh} = $memslice / 1024 / 1024;

    # Adjust cores based on 4G sort buffer size (with 25% slack)
    if (($self->{cores} * 4 * 1.25) > $self->{memh}) {
      $self->{cores} = int($self->{memh} / 4 / 1.25);
      print STDERR "Adjusting to use $self->{cores} cores\n";
    }
  }    # end passbox processing

} ## end sub set_cores

sub set_ulimit {
  # Set ulimit by checking current system limit
  #   It is recommended to increase the current system limit so the mergesort
  #   can run in a single batch rather than occur in stages due to inability to open
  #   many file handles.
  my $self = shift;

  # Set appropriate ulimit -- used for mergesort with a large number of files
  my $ulimit = `sh -c 'ulimit -n'`;
  chomp $ulimit;
  if ($ulimit > 32000) {
    $ulimit = 16384;    # I don't expect more sort pieces than this
  }
  else {
    if ($ulimit > 1000) {
      $ulimit = 1000;
    }
    else {
      $ulimit = 16
        ; # This is the default for sort so it will probably work, though I don't know why ulimit would be less than 1000
    }
  }
  $self->{ulimit} = $ulimit;
  print STDERR "Using merge batch limit of $self->{ulimit} open files\n";
} ## end sub set_ulimit

1;
