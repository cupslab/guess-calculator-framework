# Class for processing configuration options for create_training_and_test_corpora.pl
#
# This file defines an object which the main Perl file will use to determine which
#   options are enabled.  The Getopt module is used to parse the command-line and
#   the process_command_line function validates the options and sets the correct
#   public variables in this object's hash.
#
# Written by Saranga Komanduri
package CorporaOptionsHandler;

use strict;
use warnings;
use Getopt::Std;
use Cwd qw(abs_path getcwd);
use lib 'binaries';
use MiscUtils;

sub new {
  # Constructor
  my ($class, $args) = @_;
  my $self = {
    # Options which control execution
    cores         => 1,
    coresSet      => 0,
    workingDirSet => 0,
    stableMode    => 1,

    # File / folder paths
    configFile => '',
    workingDir => getcwd() . '/working',

    # Miscellaneous variables
    start_overall => time(),
    version       => $args->{VERSION}
  };
  return bless $self, $class;
}

sub print_usage {
  # Print usage in case of invalid command-line arguments
  my ($self, $arg) = @_;
  print STDERR "missing $arg \n" if defined $arg;

  print STDERR << "EOF";
  Create training and test corpora for the guess calculator framework v$self->{version}

  $0 [-h] [-b] [-n #] [-d dir] -- <path to corpora configuration file>

  this script has one required argument that specifies path to corpora configuration file  (not to be confused with condition configuration files used with the guess calculator framework)

  this script has optional arguments:
    -h   this help
    -n   specify maximum number of cores to use for any parallel sections (note that this applies only to this script and not to future uses of the guess calculator framework)
    -d   specify directory for temporary / working files (defaults to "./working")
    -b   random mode (random operations DO NOT use a set seed for deterministic results)

  example:
    $0 -o -- config/basic_experiment1.dat

EOF

  exit;
} ## end sub print_usage

sub process_command_line {
  # Process command line options and set config variables in object hash appropriately
  my $self = shift;

  my %opts;
  getopts('hbd:n:', \%opts);

  # On -h just print usage and quit
  $self->print_usage() if defined $opts{h};

  # Check that file arguments are specified and then check that they exist
  # Store condition files from remainder of ARGV (GetOpts modifies ARGV)
  if (scalar(@ARGV) == 1) {
    $self->{configFile} = abs_path($ARGV[0]);
    print STDERR "Using configuration file: $self->{configFile}\n";
    MiscUtils::test_exist($self->{configFile});
  }
  else {
    die
      "None or too many corpora configuration files found on command line (should be only one, "
      . scalar(@ARGV)
      . " found)!\n"
  }

  # Check optional working directory argument
  # Map command-line option to getopts ID to config variable key
  my @optDirArguments = (
    [qw/-d d workingDir/]
  );
  foreach my $optarg (@optDirArguments) {
    if (defined $opts{ @$optarg[1] }) {
      if (-d $opts{ @$optarg[1] }) {        
        $self->{ @$optarg[2] } = abs_path( $opts{ @$optarg[1] } );
      }
      else {
        # abs_path can't resolve nonexistent directories :(
        $self->{ @$optarg[2] } = abs_path('.') . '/' . $opts{ @$optarg[1] }; 
      }
    }
  }

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
        die
          "Option @$optarg[0] was malformed!  Read in as $opts{@$optarg[0]}\n";
      }
    }
  }

  # Set config variables for those options which have no associated value or when we want to store that a value was specified (as in number of cores)
  my @optBooleans = (
    [qw/n coresSet 1/],
    [qw/d workingDirSet 1/],
    [qw/b stableMode 0/]
  );
  foreach my $optbool (@optBooleans) {
    $self->{ @$optbool[1] } = @$optbool[2] if defined $opts{ @$optbool[0] };
  }
} ## end sub process_command_line

sub set_cores {
  # Set the number of cores to use based on the number of cores available
  my $self = shift;

  if (!$self->{coresSet}) {
    chomp(my $cores = `cat /proc/cpuinfo | grep processor | wc -l`);
    # Validate $cores number from wc -l
    if (!$cores =~ /^\d+$/) {
      print STDERR "odd, cpuinfo shows this for cores: $cores\n";
    }
    else {
      if ($cores > 3) {
        $self->{cores} = $cores -
          2;    # reserve two cores so that the management thread gets cputime
      }
    }
  }

  print STDERR "Maximum number of parallel processes: $self->{cores}\n";
} ## end sub set_cores

1;
