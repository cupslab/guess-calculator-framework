# Package for processing configuration options for create_training_and_test_corpora.pl
#
# Written by Saranga Komanduri
package CorporaUtils;

use strict;
use warnings;
use File::Temp;
use Cwd qw(abs_path getcwd);
use lib 'binaries';
use MiscUtils qw(run_cmd);

my %UTILS = (
  'wordfreqprocessor'    => "./process_wordfreq.py"
);

sub check_utils {
  foreach my $util (values %UTILS) {
    print STDERR "Checking for executable ${util}...";
    MiscUtils::test_exec($util);
    print STDERR "passed!\n";
  }
}

sub load_and_check_corpora_config {
  # Load configuration file into global object CorporaConfig,
  #   check that it contains a "%TOP" hash, and check for files.
  # Die on any error.

  my $filename  = shift;
  my $NAMESPACE = "CorporaConfig";

  MiscUtils::read_cfg($filename, $NAMESPACE);
  if (!scalar(%CorporaConfig::TOP)) {
    die "%TOP not found or malformed in config file: $filename\n";
  }
  else {
    print STDERR "Validating config file: $filename\n";
  }

  # Check for required config parameters
  foreach my $param ('name', 'training_configuration', 'testing_configuration')
  {
    if (!defined $CorporaConfig::TOP{$param}) {
      die "  Configuration parameter $param not found in config file.\n";
    }
  }
  if (!MiscUtils::valid_condition_name($CorporaConfig::TOP{'name'})) {
    die "  Condition name: $CorporaConfig::TOP{'name'} "
      . "contains invalid characters!\n";
  }

  # If tokenizers are defined, check that they exist
  foreach my $tokenizer ('tokenize_structures_method', 'tokenize_tts_method')
  {
    if (defined $CorporaConfig::TOP{$tokenizer}) {
      if (!-e $CorporaConfig::TOP{$tokenizer}) {
        die "  Couldn't find tokenizer $CorporaConfig::TOP{$tokenizer} "
          . "in top level of configuration file.\n";
      } else {
        MiscUtils::test_exec($CorporaConfig::TOP{$tokenizer});
      }
    }
  }

  # For each class of training corpus, iterate through sources and check for required paramters
  my %train_config = %{ $CorporaConfig::TOP{'training_configuration'} };
  foreach my $class ('structs', 'digsym', 'strings') {
    foreach my $sourceref (@{ $train_config{$class} }) {
      my %source = %{$sourceref};
      foreach my $param ('name', 'filename') {
        if (!defined $source{$param}) {
          die "  Configuration parameter $param not found in "
            . "source in training_configuration->$class.\n";
        }
      }
      if (!-e $source{'filename'}) {
        die "  Couldn't find file $source{'filename'} in source "
          . "$source{'name'} in training_configuration->$class.\n";
      }
      if (defined $source{'filter'} && !-e $source{'filter'}) {
        die "  Couldn't find filter $source{'filter'} in source "
          . "$source{'name'} in testing_configuration.\n";
      }
    }
  }

  # Perform similar check for each source in testing configuration
  my %test_config = %{ $CorporaConfig::TOP{'testing_configuration'} };
  foreach my $sourceref (@{ $test_config{'sources'} }) {
    my %source = %{$sourceref};
    foreach my $param ('name', 'filename') {
      if (!defined $source{$param}) {
        die "  Configuration parameter $param not found in source "
          . "in testing_configuration.\n";
      }
    }
    if (!MiscUtils::valid_condition_name($source{'name'})) {
      die "  Configuration parameter 'name' contains invalid characters!\n";
    }
    if (!-e $source{'filename'}) {
      die "  Couldn't find file $source{'filename'} in source $source{'name'} "
        . "in testing_configuration.\n";
    }
    if (defined $source{'filter'} && !-e $source{'filter'}) {
      die "  Couldn't find filter $source{'filter'} in source $source{'name'} "
        . "in testing_configuration.\n";
    }
  } ## end foreach my $sourceref (@{ $test_config...})
} ## end sub load_and_check_corpora_config

sub process_test_source {
  # Inputs: Hash reference to a single source from testing_configuration hash,
  #   working directory, and File::Temp object for processed source
  # Processes source file in stages, through filter and conversion if needed
  # NOTE: This subroutine uses the %remainders hash by reference, so it has
  #   the side-effect of modifying this hash.  This is intended.
  #
  # die on error
  my %source       = %{ shift @_ };
  my $remainderref = shift;
  my $workingdir   = shift;
  my $finalFile    = shift;
  die "Not enough arguments to process_test_source!\n" if (!defined $finalFile);

  my $sourcefile = abs_path($source{'filename'});
  $source{'description'} ||= '';
  print STDERR "Processing test set $source{'name'}: $source{'description'}"
    . " with source file $sourcefile.\n";

  # Invariant: When outside of a block, $currentsource should always point at the latest version of the final file
  my $currentsource      = $sourcefile;
  my @tempfilestodestroy = ();

  # Handle filter
  if ($source{'filter'}) {
    my $filter = abs_path($source{'filter'});
    print STDERR "Applying filter $filter to source\n";
    ( my $tmpFile = File::Temp->new(
        TEMPLATE => 'filteredXXXXXX',
        UNLINK   => 0,
        DIR      => $workingdir,
        SUFFIX   => '.test'
        )
      )
      or die
      "Could not create temporary file for filter in process_test_source - $@!\n";
    run_cmd("perl \"$filter\" \"$currentsource\" > \"$tmpFile\"");
    close($tmpFile);
    my $lines = MiscUtils::count_lines($tmpFile->filename);    
    print STDERR "$lines lines remaining after filter.\n";
    if ($lines == 0) {
      die 
        "No lines remaining.  There might be a problem with your filter!";
    }
    $currentsource = $tmpFile->filename;
    push(@tempfilestodestroy, $tmpFile);
  }

  # Handle counting and remainder
  if ($source{'count'}) {
    my $count = int($source{'count'});
    if ($count > 0) {
      print STDERR "Randomly drawing $count lines from $currentsource\n";

      # Create file of lines to keep
      ( my $keepFile = File::Temp->new(
          TEMPLATE => 'countedXXXXXX',
          UNLINK   => 0,
          DIR      => $workingdir,
          SUFFIX   => '.test'
          )
        )
        or die
        "Could not create temporary file for filter in process_test_source - $@!\n";

      # Create remainder file if needed
      my $remainderFile = undef;
      if ($source{'save_remainder'}) {
        ( $remainderFile = File::Temp->new(
            TEMPLATE => 'remainderXXXXXX',
            UNLINK   => 0,
            DIR      => $workingdir,
            SUFFIX   => '.test'
            )
          )
          or die
          "Could not create temporary file for filter in process_test_source - $@!\n";
        if (exists($remainderref->{$sourcefile})) {
          die
            "The same sourcefile was specified multiple times (with save_remainder set) in testing_configuration! Aborting...\n";
        }
        else {
          print STDERR
            "Creating remainder file $remainderFile for remainder of $currentsource.\n";
        }
        $remainderref->{$sourcefile} =
          $remainderFile;    # Save for later use in training
      } ## end if ($source{'save_remainder'...})

      open(my $fh, "<", $currentsource)
        or die "Unable to open $currentsource in process_test_source - $?!\n";
      # Use the reservoir sampling algorithm
      my @keptlines      = ();
      my $keptlinescount = 0;
      while (my $line = <$fh>) {
        if ($keptlinescount < $count) {
          push(@keptlines, $line);
          $keptlinescount++;
        }
        else {
          # Decide whether to keep or discard the line
          my $listindex = int(rand($.));
          if ($listindex < scalar(@keptlines)) {
            # Replace this line
            print $remainderFile $keptlines[$listindex]
              if ($source{'save_remainder'});
            $keptlines[$listindex] = $line;
          }
          else {
            print $remainderFile $line if ($source{'save_remainder'});
          }
        }
      }
      print $keepFile join('', @keptlines);
      close($keepFile);
      $currentsource = $keepFile->filename;
      print STDERR scalar(@keptlines) . " lines written to $currentsource.\n";
      push(@tempfilestodestroy, $keepFile);
      close($remainderFile) if ($source{'save_remainder'});
    } ## end if ($count > 0)
    else {
      print STDERR
        "Ignoring non-positive count value $count for test source $source{'name'}!\n";
    }
  } ## end if ($source{'count'})

  # Handle conversion
  if ($source{'convert_from_single_column'}) {
    print STDERR
      "Converting source $currentsource to three-column lookup format\n";
    ( my $tmpFile = File::Temp->new(
        TEMPLATE => 'convertedXXXXXX',
        UNLINK   => 0,
        DIR      => $workingdir,
        SUFFIX   => '.test'
        )
      )
      or die
      "Could not create temporary file for filter in process_test_source - $@!\n";
    open(my $fh, "<", $currentsource)
      or die "Unable to open $currentsource in process_test_source - $?!\n";
    while (my $pwd = <$fh>) {
      print $tmpFile "no_user\t$source{'name'}\t$pwd";
    }
    close($tmpFile);
    close($fh);
    $currentsource = $tmpFile->filename;
    push(@tempfilestodestroy, $tmpFile);
  } ## end if ($source{'convert_from_single_column'...})

  # Copy current file to final file
  run_cmd("cp \"$currentsource\" \"$finalFile\"");

  # Clean up temp files
  foreach my $tmpFile (@tempfilestodestroy) {
    print STDERR "Deleting temporary file $tmpFile\n";
    $tmpFile->unlink_on_destroy(1);
    $tmpFile->DESTROY();
  }
  print STDERR "\n";
} ## end sub process_test_source

sub create_test_set {
  # Inputs: Hash reference to the testing_configuration hash,
  #   working directory, file for final lookup set
  # die on error
  my %test_config   = %{ shift @_ };
  my $remaindersref = shift;
  my $workingdir    = shift;
  my $finalfilename = shift;
  die "Not enough arguments to create_test_set!\n" if (!defined $finalfilename);

  # Delete and touch final file
  run_cmd "rm -f \"$finalfilename\"" if (-e $finalfilename);
  run_cmd "touch \"$finalfilename\"";

  # sources is a list of hashes
  foreach my $sourceref (@{ $test_config{'sources'} }) {
    # Process each source to separate file and combine them all at the end
    ( my $tmpFile = File::Temp->new(
        TEMPLATE => 'tempXXXXXX',
        DIR      => $workingdir,
        SUFFIX   => '.test'
        )
    ) or die "Could not create temporary file in create_test_set - $@!\n";
    process_test_source($sourceref, $remaindersref, $workingdir, $tmpFile);
    run_cmd "cat \"$tmpFile\" >> \"$finalfilename\"";
  }
} ## end sub create_test_set


sub get_weight {
  # Use the wordfreq processor to measure the total weight of the given file
  #   and return it.
  # Assumes file is in plaintext word-freq format.
  my $srcfile = shift;

  my @returnedoutput = 
    run_cmd(
      "$UTILS{'wordfreqprocessor'} --only-measure -p \"$srcfile\""
    );
  my $weight = $returnedoutput[0];
  chomp $weight;
  print STDERR "Measured weight of $weight from $srcfile.\n";
  return($weight);
}


sub process_training_source {
  # Inputs: Hash reference to a single source from testing_configuration hash,
  #   working directory, and File::Temp object for processed source
  # Processes source file in stages, through wordfreq processor and filter if needed
  # die on error
  my %source       = %{ shift @_ };
  my $remainderref = shift;
  my $combinedFile = shift;
  my $workingdir   = shift;
  my $finalFile    = shift;
  die "Not enough arguments to process_training_source!\n"
    if (!defined $finalFile);

  my $sourcefile = abs_path($source{'filename'});
  print STDERR
    "Processing training source $source{'name'} with source file $sourcefile.\n";

  # Invariant: When outside of a block, $currentsource should always point at
  #   the latest version of the final file.
  my $currentsource = $sourcefile;
  if ($source{'use_remainder'}) {
    # If use_remainder is set, the sourcefile is actually a key to the real
    #   source, which is stored in the remainders hash.
    if (exists($remainderref->{$sourcefile})) {
      $currentsource = $remainderref->{$sourcefile}->filename;
      if (-e $currentsource) {
        print STDERR
          "Found use_remainder flag!  Using $currentsource as source file.\n"
      }
      else {
        die "Remainder file $currentsource not found but should exist!\n"
      }
    }
    else {
      die
        "use_remainder specified for source $source{'name'} but no remainder found!  Did you forget to specify save_remainder in the corresponding test source?\n";
    }
  }
  my @tempfilestodestroy = ();

  # Process source option - weight
  my $weight = 1;
  my $adjustweight = 0;
  if (($source{'weight'} =~ /^\d+\.?\d*$/)) {
    $weight = $source{'weight'};
  } else {
    # Check for an 'x' at the end of the weight -- this indicates a multiplier
    if (($source{'weight'} =~ /^(\d+\.?\d*)x$/)) {
      $adjustweight = $1;
    }
  }

  # Process source option - ID
  my $idstr = "";
  $idstr = "-n $source{'ID'}" if (defined $source{'ID'});

  # Always run the source through the wordfreq processor first to normalize it
  print STDERR "Preprocessing source $currentsource\n";
  ( my $tmpFile = File::Temp->new(
      TEMPLATE => 'processedXXXXXX',
      UNLINK   => 0,
      DIR      => $workingdir,
      SUFFIX   => '.train'
      )
    )
    or die
    "Could not create temporary file for wordfreq processor in process_training_source - $@!\n";
  run_cmd(
    "$UTILS{'wordfreqprocessor'} -v -w $weight $idstr \"$currentsource\" \"$tmpFile\""
  );
  close($tmpFile);
  $currentsource = $tmpFile->filename;
  push(@tempfilestodestroy, $tmpFile);

  # Handle filter
  if ($source{'filter'}) {
    my $filter = abs_path($source{'filter'});
    print STDERR "Applying filter $filter to source $currentsource\n";
    ( my $tmpFile = File::Temp->new(
        TEMPLATE => 'filteredXXXXXX',
        UNLINK   => 0,
        DIR      => $workingdir,
        SUFFIX   => '.train'
        )
      )
      or die
      "Could not create temporary file for filter in process_training_source - $@!\n";
    run_cmd("perl \"$filter\" \"$currentsource\" > \"$tmpFile\"");
    close($tmpFile);
    my $lines = MiscUtils::count_lines($tmpFile->filename);
    print STDERR "$lines lines remaining after filter.\n";
    $currentsource = $tmpFile->filename;
    push(@tempfilestodestroy, $tmpFile);
  }

  # Handle weight multiplier
  if ($adjustweight) {
    # Setting the correct weight requires multiple steps:
    #   1. Use wordfreq processor to get the weight of the currentsource
    #      and current combinedfile
    #   2. Calculate multiplier
    #   3. Use wordfreq processor to set weight of currentsource
    my $srcweight = get_weight($currentsource);
    my $currenttotal = get_weight($combinedFile->filename);
    my $multiplier = $currenttotal / $srcweight;
    print STDERR "Adjusting weight of source $currentsource by $multiplier\n";
    ( my $tmpFile = File::Temp->new(
        TEMPLATE => 'weightedXXXXXX',
        UNLINK   => 0,
        DIR      => $workingdir,
        SUFFIX   => '.train'
        )
      )
      or die
      "Could not create temporary file for weight adjustment in process_training_source - $@!\n";
    run_cmd(
      "$UTILS{'wordfreqprocessor'} -v -p -w $multiplier \"$currentsource\" \"$tmpFile\""
    );
    close($tmpFile);
    $currentsource = $tmpFile->filename;
    push(@tempfilestodestroy, $tmpFile);    
  }

  # Copy current file to final file
  run_cmd("cp \"$currentsource\" \"$finalFile\"");

  # Clean up temp files
  foreach my $tmpFile (@tempfilestodestroy) {
    print STDERR "Deleting temporary file $tmpFile\n";
    $tmpFile->unlink_on_destroy(1);
    $tmpFile->DESTROY();
  }
  print STDERR "\n";
} ## end sub process_training_source

sub create_training_set {
  # Inputs: Array reference to array of sources,
  #   working directory, file for final set
  # Final file is assumed to be a .gz file
  # die on error
  my @sources       = @{ shift @_ };
  my $remaindersref = shift;
  my $workingdir    = shift;
  my $finalfilename = shift;
  die "Not enough arguments to create_training_set!\n"
    if (!defined $finalfilename);

  # Set up final files
  run_cmd "rm -f \"$finalfilename\"" if (-e $finalfilename);
  # Since final file is a .gz, we work with a temporary plaintext file until the end
  # Invariant: $combinedFile is always a valid plaintext wordfreq file (though it may be empty)
  ( my $combinedFile = File::Temp->new(
      TEMPLATE => 'combinedXXXXXX',
      DIR      => $workingdir,
      SUFFIX   => '.train'
      )
  ) or die "Could not create temporary file in create_training_set - $@!\n";

  foreach my $sourceref (@sources) {
    # Process each source to separate file and combine them all at the end
    ( my $tmpFile = File::Temp->new(
        TEMPLATE => 'tempXXXXXX',
        DIR      => $workingdir,
        SUFFIX   => '.train'
        )
    ) or die "Could not create temporary file in create_training_set - $@!\n";
    # Pass current combinedFile, in case the source weight is a multiplier
    #   (specified with an "x", e.g., weight => '10x')
    process_training_source($sourceref, $remaindersref, $combinedFile,
      $workingdir, $tmpFile);
    run_cmd "cat \"$tmpFile\" >> \"$combinedFile\"";
  }

  # The wordfreq processor will automatically merge duplicate password entries
  #   and aggregate their weights.
  # With the -g flag, it will also make a .gz file.
  # The -p flag indicates that the input is in plaintext wordfreq format.
  run_cmd(
    "$UTILS{'wordfreqprocessor'} -v -p -g \"$combinedFile\" \"$finalfilename\""
  );
} ## end sub create_training_set

sub tokenize_structures {
  # This function takes a processed structures corpus and working directory
  #   and tokenizes the corpus using the given script.  On exit, the input
  #   corpus on disk is replaced by the tokenized version.
  my $tokenizer     = shift;
  my $tokenizerargs = shift;
  $tokenizerargs = "" if (!$tokenizerargs);
  my $finalfilename = shift;
  my $workingdir    = shift;
  die "Not enough arguments to tokenize_structures!\n" if (!defined $workingdir);


  print STDERR "Starting tokenization of structures...";
  ( my $tmpFile = File::Temp->new(
      TEMPLATE => 'tempXXXXXX',
      DIR      => $workingdir,
      SUFFIX   => '.tokens'
      )
  ) or die "Could not create temporary file during tokenizing - $@\n";
  # Run the tokenizer
  run_cmd "\"$tokenizer\" $tokenizerargs \"$finalfilename\" > \"$tmpFile\"";
  close($tmpFile);
  print STDERR "done!\n";

  # Finally, run the wordfreq processor as in create_training_set and overwrite
  #   the original file.
  print STDERR "Running wordfreq processor on tokenized structures...";
  run_cmd(
    "$UTILS{'wordfreqprocessor'} -v -i -p -g \"$tmpFile\" \"$finalfilename\""
  );
  print STDERR "done!\n";
} ## end sub tokenize_structures

sub tokenize_terminals {
  # This function takes a processed terminals corpus, working directory,
  #   and tokenized structures corpus and runs the given tokenizer.
  # The tokenizer is assumed to work similarly to the structures tokenizer
  #   in that it separates tokens with the '\x01' character.  This function
  #   will split these tokens to separate lines before the file is moved
  #   to the next stage.
  my $tokenizer     = shift;
  my $tokenizerargs = shift;
  $tokenizerargs = "" if (!$tokenizerargs);
  my $finalfilename = shift;
  my $structures    = shift;
  my $workingdir    = shift;
  die "Not enough arguments to tokenize_terminals!\n" if (!defined $workingdir);

  print STDERR "Starting tokenization of terminals...";
  ( my $tmpFile = File::Temp->new(
      TEMPLATE => 'tempXXXXXX',
      DIR      => $workingdir,
      SUFFIX   => '.tokens'
      )
  ) or die "Could not create temporary file during tokenizing - $@\n";
  # Run the tokenizer
  run_cmd "\"$tokenizer\" $tokenizerargs \"$finalfilename\" \"$structures\"  > \"$tmpFile\"";
  close($tmpFile);
  print STDERR "done!\n";

  # Ultimately we want to split on the token separator character and 
  # place tokens on separate lines with frequencies
  print STDERR "Strings now tokenized, filtering...";
  ( my $tmpFile2 = File::Temp->new(
      TEMPLATE => 'splittokensXXXXXX',
      DIR      => $workingdir,
      SUFFIX   => '.tokens'
      )
  ) or die "Could not create temporary file during tokenizing - $@\n";
  open(my $fh, "<", $tmpFile)
    or die "Unable to open $tmpFile in tokenize_terminals - $?!\n";
  while (my $line = <$fh>) {
    chomp $line;
    my @fields  = split /\t/, $line;
    my $pwd     = $fields[0];
    my $incount = $fields[1];
    my $inID    = $fields[2];

    while ($pwd =~ /([^\x01]+)/g) {
      my $found = lc $1;
      print $tmpFile2 "$found\t$incount\t$inID\n"
    }
  }
  close($tmpFile2);
  close($fh);
  print STDERR "done!\n";

  # Finally, run the wordfreq processor as in create_training_set and overwrite
  #   the original file.
  print STDERR "Running wordfreq processor on filtered strings...";
  run_cmd(
    "$UTILS{'wordfreqprocessor'} -v -i -p -g \"$tmpFile2\" \"$finalfilename\""
  );
  print STDERR "done!\n";
} ## end sub tokenize_strings

sub combine_terminal_corpora {
  # This function combines the tokenized and untokenized terminal corpora,
  #   performing a simple copy if only one is found.
  my $tokenizedstructures  = shift;
  my $tokenizedterminals   = shift;
  my $untokenizedterminals = shift;
  my $finalfilename        = shift;
  my $workingdir    = shift;
  die "Not enough arguments to combine_terminal_corpora!\n" if (!defined $workingdir);
  if (!$tokenizedstructures) {
    die "Structures not found! A structure source must be defined!\n";
  }

  # Combine the corpora
  print STDERR "Combining structures with tokenized and untokenized terminals...";
  ( my $tmpFile = File::Temp->new(
      TEMPLATE => 'tempXXXXXX',
      DIR      => $workingdir,
      SUFFIX   => '.tokens'
      )
  ) or die "Could not create temporary file when combining terminal sources! - $@\n";
  # Cat the corpora together
  run_cmd "zcat \"$tokenizedstructures\" > \"$tmpFile\"";
  if (-e $tokenizedterminals) {
    run_cmd "zcat \"$tokenizedterminals\" >> \"$tmpFile\"";
  }
  if (-e $untokenizedterminals) {
    run_cmd "zcat \"$untokenizedterminals\" >> \"$tmpFile\"";
  }
  close($tmpFile);
  print STDERR "done!\n";  
  # Finally, run the wordfreq processor as in create_training_set and overwrite
  #   the original file.
  print STDERR "Running wordfreq processor on combined corpora...";
  run_cmd(
    "$UTILS{'wordfreqprocessor'} -v -i -p -g \"$tmpFile\" \"$finalfilename\""
  );
  print STDERR "done!\n";
}


sub create_config_file {
  # Inputs: 1) Hash reference to corpora configuration hash because this has a
  #   number of parameters that could be used in the config file
  #   2) Hash reference to final file locations,
  #   3) filename for output config file
  my $hashref    = shift;
  my %config     = %{$hashref};
  my $hashref2   = shift;
  my %finalfiles = %{$hashref2};
  my $workingdir = shift;
  die "Not enough arguments to create_config_file!\n" if (!defined $workingdir);

  print STDERR "\nWriting config file...";
  open my $configfh, ">", $finalfiles{'config'}
    or die "Couldn't open config file $finalfiles{'config'} for writing!\n";
  # Add standard / required values
  print $configfh <<"EOF";
%CFG = (
  'name' => '$config{'name'}',
  'desc' => 'Automatically created config file for $config{'description'}',
  'structures' => '$finalfiles{'structures'}',
  'terminals' => '$finalfiles{'terminals'}',
  'lookup' => '$finalfiles{'lookup'}',
EOF

  # Add optional values - pass them directly from the corpora config
  my @optvalues = qw/cutoffmin cutoffmax totalquantlevels quantizeriters generate_unseen_terminals/;
  foreach my $opt (@optvalues) {
    if (defined $config{$opt}) {
      print $configfh "  '$opt' => '$config{$opt}',\n"
    }
  }

  print $configfh ");";
  my $t = localtime;
  print STDERR "done!\n\nFinished run at $t.\n\n\n";
} ## end sub create_config_file

1;
