#!/usr/bin/perl
###############################################################################
# create_training_and_test_corpora.pl
#
# This script processes input datasets, which can be in many different formats,
#   and processes them into corpora that can be used to train the guess
#   calculator framework.
# Though it adds complexity, this script also processes input test datasets in
#   a similar manner.  Password guessing experiments are often run using a single
#   input set that is split into training and test sets.  This script supports
#   this operation, as well as allowing training and test sets to be created
#   independently.
#
# The inputs to the script are: a configuration file that specifies input
#   files and their weights, and a path to a directory in which to place
#   temporary and final files.
#
# The configuration file is actually executable Perl code, loaded using Perl's
#   Safe::rdo method.  This combines two unsafe practices: executing data, and
#   loading data into a global variable (Safe loads data into a globally
#   accessible namespace).  This should be changed at some future point.  Use
#   with caution.
#
# The configuration file will identify several input datafiles.  Training files
#   can be in any format recognized automatically by process_wordfreq.py, which
#   is currently either gzipped <word><frequency><sources> format, or plain
#   one-password-per-line format.  Test files must be in either three-column,
#   plaintext <user id><condition><password> format, or plain one-password-per-line
#   format.  In the latter case, set the convert_to_three_column flag in
#   the configuration.
#
# Written by Saranga Komanduri, based on ideas contributed by Michelle Mazurek
# Formatted by perltidy
use strict;
use warnings;

# Load Perl modules
use Cwd 'abs_path';
use File::Temp;

# Load local modules
use CorporaOptionsHandler;
use CorporaUtils;
use lib 'binaries';
use MiscUtils;

my $VERSION = "1.00";    # Tue Apr 29 14:54:11 2014

#####
##### Initialization
#####

my $t = localtime;
print STDERR "Beginning run at $t.\n";

# Process command-line options
my $options = CorporaOptionsHandler->new({ VERSION => $VERSION });
$options->process_command_line();
$options->set_cores();

# Check working directory
if (-d $options->{workingDir}) {
  print STDERR "Found working directory $options->{workingDir}\n";
}
else {
  system("mkdir -p \"$options->{workingDir}\"") == 0
    or die "Could not create working directory: $?";
  print STDERR "Created working directory $options->{workingDir}\n";
}

# Set random seed if stable mode is enabled
if ($options->{stableMode}) {
  srand(6388031304603142746);  # just a random number
} # else Perl will pseudorandomly choose a seed

# Load configuration file into global object CorporaConfig,
#   and perform various checks
CorporaUtils::load_and_check_corpora_config($options->{configFile});
CorporaUtils::check_utils();

# Check that the "final" files do not exist
my $condition  = $CorporaConfig::TOP{'name'};
my %finalfiles = (
  'structures'            => "$options->{workingDir}/$condition.structures.gz",
  'terminals'             => "$options->{workingDir}/$condition.terminals.gz",
  'lookup'                => "$options->{workingDir}/$condition.test",
  'config'                => "$options->{workingDir}/${condition}_config.dat",
  # Note: the following files are used for temporary storage and are not "final"
  'tokenized terminals'   => "$options->{workingDir}/$condition.tts.gz",
  'untokenized terminals' => "$options->{workingDir}/$condition.uts.gz"
);

foreach my $file (values %finalfiles) {
  if (-e $file) {
    warn "File $file was already found! This script will overwrite it.\n";
  }
}

#####
##### Process files into corpora
#####

# NOTE: Test sets must be created before training sets.
#   This is because sometimes training sets consist of the remainder of a test
#   set after some number of passwords are removed.
# NOTE: Remainders will not be in the same order as the source due to use of
#   a random sampling algorithm, but this is not expected to be problematic.
my %remainders = ();    # Use a hash to keep track of the remainders
my %test_config = %{ $CorporaConfig::TOP{'testing_configuration'} };
CorporaUtils::create_test_set(
  \%test_config, \%remainders,
  $options->{workingDir},
  $finalfiles{'lookup'}
);

# Training sets can be produced in any order, once test sets are complete
my %training_config = %{ $CorporaConfig::TOP{'training_configuration'} };
# Map hash members of training_config to files in finalfiles
my %trainingsets = (
  'structure sources'            => 'structures',
  'tokenized terminal sources'   => 'tokenized terminals',
  'untokenized terminal sources' => 'untokenized terminals'
);
foreach my $set (keys %trainingsets) {
  if ($training_config{$set}) {
    CorporaUtils::create_training_set(
      \@{ $training_config{$set} },
      \%remainders,
      $options->{workingDir},
      $finalfiles{ $trainingsets{$set} }
    );
  }
}

# Clean up remainders.  These should be File::Temp objects.
foreach my $tmpFile (values %remainders) {
  print STDERR "Deleting temporary file $tmpFile\n";
  $tmpFile->unlink_on_destroy(1);
  $tmpFile->DESTROY();
}

# Tokenize corpora if needed
#
# Structures must be tokenized first, because the terminal tokenizer might
# use them to parse and tokenize the terminal strings
if ($CorporaConfig::TOP{'tokenize_structures_method'} &&
    $CorporaConfig::TOP{'tokenize_structures_method'} ne 'none') {
  CorporaUtils::tokenize_structures(
    $CorporaConfig::TOP{'tokenize_structures_method'},
    $CorporaConfig::TOP{'tokenize_structures_arguments'},
    $finalfiles{'structures'},
    $options->{workingDir}
  );
}
if ($CorporaConfig::TOP{'tokenize_tts_method'} &&
    $CorporaConfig::TOP{'tokenize_tts_method'} ne 'none') {
  CorporaUtils::tokenize_terminals(
    $CorporaConfig::TOP{'tokenize_tts_method'},
    $CorporaConfig::TOP{'tokenize_tts_arguments'},
    $finalfiles{'tokenized terminals'},
    $finalfiles{'structures'},
    $options->{workingDir}
  );
}

# Finally, combine the terminals files
CorporaUtils::combine_terminal_corpora(
  $finalfiles{'structures'},
  $finalfiles{'tokenized terminals'},
  $finalfiles{'untokenized terminals'},
  $finalfiles{'terminals'},
  $options->{workingDir}
);

# If desired, uniformize strings in the terminals to emulate original Weir
# behavior (assuming you have also used a character-class tokenizer)
if ($CorporaConfig::TOP{'uniformize_strings'}) {
  system("perl uniformize_strings.pl \"$finalfiles{'terminals'}\"") == 0
    or die "Failed uniformizing strings! $?";
  print STDERR "Finished uniformizing strings.\n";  
}

# Generate iterate_experiments config file
CorporaUtils::create_config_file(
  \%CorporaConfig::TOP, \%finalfiles,
  $options->{workingDir}
);
