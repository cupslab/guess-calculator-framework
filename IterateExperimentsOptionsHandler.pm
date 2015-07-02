# Object to hold configuration options for iterate_experiments.pl and process command-line options
#
# Written by Saranga Komanduri
package IterateExperimentsOptionsHandler;

use strict;
use warnings;
use Getopt::Std;
use Cwd 'abs_path';
use lib 'binaries';
use MiscUtils;

sub new {
  # Constructor
  my ($class, $args) = @_;
  my $self = {
    # Options which control execution
    cores                    => 1,
    coresSet                 => 0,
    onlyMakeDictionaries     => 0,
    deleteConditionDirectory => 1,
    deleteMode               => 1,
    pauseMode                => 0,
    mustRebuild              => 0,
    stableMode               => 1,

    # File / folder paths
    conditions => [],
    rootName   => 'calculator'
    , # Note that this is currently not passed to single_run so change the corresponding value there if you change this

    # Miscellaneous variables referenced in single_run
    start_overall => time(),
    exitcode      => 5
    , # indicates MAX_TIME runtime exceeded, must match expected exit code in single_run.pl
    version => $args->{VERSION}
  };
  return bless $self, $class;
} ## end sub new

sub print_usage {
  # Print usage in case of invalid command-line arguments
  my ($self, $arg) = @_;
  print STDERR "missing $arg \n" if defined $arg;

  print STDERR << "EOF";
  Guess calculator framework automation script v$self->{version}

  $0 [-hobkpn] -- <path to condition config file 1> <path to condition config file 2> ...

  this script has optional arguments:
    -h   this help
    -o   remove any existing directories for conditions, only create calculator.zip (stage1), then stop
    -k   don't delete the condition directory after building lookup table (warning: this might leave sensitive data on the machine)
    -p   pause for input from STDIN before starting parallel operations
    -n   specify number of cores for parallel lookup
    -b   random mode (random operations DO NOT use a set seed for deterministic results)
    -D   (for debugging only) don't delete any intermediate files produced while generating the lookup table

  example:
    $0 -o -- working/basic_experiments.dat

EOF

  exit 1;
} ## end sub print_usage

sub process_command_line {
  # Process command line options and set config variables in object hash appropriately
  my $self = shift;

  my %opts;
  getopts('hobkpDn:', \%opts);

  # On -h just print usage and quit
  $self->print_usage() if defined $opts{h};

  # Set config variables for those options that are specified (default values were set in the constructor)
  # Map command-line option to config variable key
  my @optArguments = (
    [qw/n cores ^\\d+$/]
  );
  foreach my $optarg (@optArguments) {
    if (defined $opts{ @$optarg[0] }) {
      if ($opts{ @$optarg[0] } =~ /@$optarg[2]/) {
        $self->{ @$optarg[1] } = $opts{ @$optarg[0] };
      }
      else {
        warn
          "Option @$optarg[0] was malformed!  Read in as $opts{@$optarg[0]}\n";
        $self->print_usage();
      }
    }
  }

  # Set config variables for those options which have no associated value or when we want to store that a value was specified (as in number of cores)
  my @optBooleans = (
    [qw/o onlyMakeDictionaries 1/],
    [qw/D deleteMode 0/],
    [qw/p pauseMode 1/],
    [qw/n coresSet 1/],
    [qw/k deleteConditionDirectory 0/],
    [qw/b stableMode 0/]
  );
  foreach my $optbool (@optBooleans) {
    $self->{ @$optbool[1] } = @$optbool[2] if defined $opts{ @$optbool[0] };
  }

  # Store condition files from remainder of ARGV (GetOpts modifies ARGV)
  if (scalar(@ARGV) > 0) {
    my @conditions = map { abs_path($_) } @ARGV;
    print "Using condition files:\n";
    foreach (@conditions)
    {
      print "$_\n";
    }
    print "\n";
    $self->{conditions} = \@conditions;
  }
  else {
    warn "No condition configuration files found on command line!\n";
    $self->print_usage();
  }
} ## end sub process_command_line

sub set_cores {
  # Set the number of cores to use based on the amount of RAM available and number of cores available, if on the Andrew machine
  my $self = shift;

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

  my $ramreserve = 2000 *
    1024;       # reserve an extra 2 GB of ram for any system overhead
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
    $mem -= $ramreserve;    #subtract out the reserve amount
  }

  $self->{memh} = $mem / 1024 / 1024;    # human readable memory amount
  my $memslice =
    $mem / $self->{cores} / 1.25;  # amount per thread, divide by 1.25 for slack
  $self->{memsliceh} = $memslice / 1024 / 1024;

  # Adjust cores based on 2G process size (with 25% slack)
  if (($self->{cores} * 2 * 1.25) > $self->{memh}) {
    $self->{cores} = int($self->{memh} / 2 / 1.25);
    print STDERR "Adjusting to use $self->{cores} cores\n";
  }

} ## end sub set_cores

1;
