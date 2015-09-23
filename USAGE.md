# Usage

**It is recommended that you read the entirety of sections 1 and 2 before running an experiment.**

## 1 Initial test

A small experiment is provided in the [minibasic6](example conditions/minibasic6) directory. After the experiment runs, you can verify the output by comparing it to the [reference results](reference results/minibasic6).

To run an experiment:

1. Process the input files into training and test corpora

  ```
  perl create_training_and_test_corpora.pl -d processed/minibasic6 minibasic6 example\ conditions/minibasic6/configs/minibasic6.dat 2>>minibasic6.log
  ```

  This command places the processed files into a directory called `processed/minibasic6` off the current directory.  If no directory is specified, files will be placed in a directory called `working`.

  The command will also generate several debug messages to stderr.  The above command captures these messages to the file `minibasic6.log`.

  This step should take less than a minute on this dataset.

2. Generate lookup table and guess numbers

  ```
  ./iterate_experiments.pl -k processed/minibasic6/minibasic6_config.dat
  ```

  The format of the file argument to this step is:

  ```
  <directory specified in step #1>/<condition name>_config.dat
  ```

  The condition name is the top-level name specified in the configuration file.  See [`%TOP{'name'} here`](#writing-a-configuration-file).

  The `-k` switch is not required.  It will save the lookup table at the completion of the run, which can be reused to look up guess numbers for additional test sets.  There are instructions for doing this [here](#step-4-looking-up-guess-numbers). These files can be terabytes in size, so it is generally not practical to save them.  You will find this file at `minibasic6-1e-12/calculator/lookuptable-1e-12`.

  The `-k` switch will create a `minibasic6-1e-12` directory. You can delete this directory if you are not interested in keeping intermediate files.

  Guess numbers will be written to a file named `lookupresults.minibasic6-1e-12`.  This file is always accompanied by a file named `totalcounts.minibasic6-1e-12` and a log file named `minibasic6-1e-12.log`.  These files are written to the root of the framework directory.  Looking at the totalcounts file, you should see that 8,590,967,875 guesses were evaluated.

  This step should take about 20 minutes.

3. Plot the guess numbers

  You can plot the output in R using the included `PlotResults.R` script.
  ```
  Rscript PlotResults.R makeplot test_minibasic6.pdf
  ```

  This will read in any lookupresults and totalcounts files in the current directory and plot guess numbers.

  Compare your results files: `lookupresults.minibasic6-1e-12` and `totalcounts.minibasic6-1e-12`, to the [reference results](reference results/minibasic6).  You should also compare the plots visually (pdf file contents will vary due to timestamps).

#### Additional example

An additional reference experiment is provided in the [minibasic6_with_improvements](example conditions/minibasic6_with_improvements) directory.  It takes about 75 minutes to run and evaluates 2,993,521,412,025 guesses.


#### Reproducible results

By default, the framework sets a random number seed in every file so that repeated runs always generate the same results.  This behavior can be turned off by passing the `-b` switch to the `create_training_and_test_corpora` and `iterate_experiments` scripts.


## 2 Conducting a policy experiment

To conduct an experiment, you need:

- Training and test data

- A configuration file

- Filters for your policy

  This is typically a set of three filters: a training filter for your policy, a "remainder" training filter which accepts all passwords that do not comply with your policy, and a test filter.  The first filter is used for structures training, while the remainder filter takes the remainder of the training data that does not comply with the target policy and makes it available as terminals.  Test filters expect plain files while training filters expect "wordfreq" files. See [here](#training-data). 

- (Optional) Tokenizers for your experiment

  You will probably use the provided [character-class tokenizer](character_class_tokenizer.py) rather than create your own.


### Training data

Training data files are in one of two formats:
* Files ending in `.gz` containing three columns are assumed to be in wordfreq format (see below).
* All other files are assumed to be in "plain" format, with one password per line.  Passwords with tabs or the `\x01` character are removed from the input.


#### Word-frequency (wordfreq) format

The framework makes extensive use of files in *wordfreq* format in training data.  This is a three-column, tab-delimited format:

```
123456    1.1bea400000000p+18    R
```

The first column is a password.

The second column is a frequency in hex-float format with the leading `0x` removed.

The third column is a tag.

Wordfreq files can be produced easily from plain files using the included `process_wordfreq.py` script.  The framework will do this for you automatically given plain input, or you can do this manually to save time over multiple experiment runs.

#### Tags

Tags are identifiers that allow you to tag input data and track which sources contributed to which guesses.  Tags are optional and can be multiple letters. Multiple tags on the same item will be concatenated together with no delimiter, so please ensure that your tags form a prefix code.  If you enable unseen terminal generation in the framework (see below), the tag `UNSEEN` is introduced and mixed with your own tags.

### Test data

Test data files must be plaintext.  They are expected to be in three-column, tab-delimited format:

```
user_id    policy_name    password
```

If provided in this format, the `user_id` and `policy_name` fields will be copied to output files. 

Test data can also be in single-column format if the `convert_from_single_column` switch is specified (see below.)


### Writing a configuration file

Configuration files must be valid Perl scripts and are evaluated using `Safe::rdo`.  Look at the [minibasic6 example](example conditions/minibasic6/configs/minibasic6.dat) for a simple example including comments. The configuration is read out of the hash `%TOP`. Below is a reference to the keys that can be included in `%TOP`.  Keys followed by * are required.

- `name`*

  The top-level name of this condition.  This name will be used when creating various files, so the character set of the string is restricted. The framework will check that this string matches `/^[\w.-]+$/`, and will throw an error otherwise.

- `description`

- `tokenize_structures_method`*

  The path to an executable (typically a script) used for tokenizing passwords.  This executable is called a tokenizer.  The last argument given to the executable will be a file to tokenize in gzipped-wordfreq format.  The framework will generate this file for you.

  Tokenizers split passwords into pieces according to some criteria and the PCFG will then combine these pieces in different combinations to make new guesses.  In a typical application of this framework, this script is a **character-class** tokenizer as in Weir's original implementation.  This is provided with the framework and you can reference it with `"./character_class_tokenizer.py"`.
    
  If any of your conditions are long password conditions and you do not have another tokenizer solution, then you might want to use the Google-ngram tokenizer which is also provided. See the FAQ for instructions on constructing a corpus for use with this tokenizer. The Google-ngram tokenizer requires about 4 GB of free RAM and will memory map the corpus file.

- `tokenize_structures_arguments`

  Any arguments for the structures tokenizer can be included in a single string here.  This will be the first argument(s) to the tokenizer and will be sent unquoted.  For example, setting this key to the value `-d 1` enables the "hybrid structures" option in `character_class_tokenizer.py` where tokenized and untokenized structures are given equal weight. 

- `tokenize_tts_method`

  The path to an executable (typically a script) used for tokenizing terminals.  This argument is only needed if the training configuration includes terminal sources to tokenize.  This is not required because terminals from tokenized structures are automatically added to the set of available terminals, regardless of whether or not other terminal sources are specified.  This is prevents an invalid PCFG specification where a nonterminal has no terminals to produce.

  The second-to-last argument to the given script will be a file to tokenize in gzipped-wordfreq format.  The last argument will be a tokenized structures file in gzipped-wordfreq format, in case the tokenizer wants to make use of information from the tokenized structures to tokenize terminals.

  A typical use of this argument uses the same executable as `tokenize_structures_method` in conjunction with a "remainder" filter.

- `tokenize_tts_arguments`

- `generate_unseen_terminals`

  Set this to 1 to enable the production of unseen terminals.  This effectively brute-forces strings in lexical order when the training data doesn't completely fill a terminal space.  This is recommended.

- `uniformize_strings`

  Set this to 1 to have probabilities in the final terminals corpus rewritten so that all alphabetic terminals are assigned uniform probability.  Mixed-class terminals and non-alphabetic terminals are unaffected.  This is used in conjunction with a character-class tokenizer to emulate the behavior of Weir's original implementation.

- `totalquantlevels`

  The total number of levels used by the quantizer, across all terminals of the grammar.  Each nonterminal must receive at least one level for its terminals, and this number specifies the number of additional levels to assign.  The default value is 500.

  Levels are allocated based on probability, so nonterminals with high probability are allocated more levels than those with a lower probability.  This strategy helps reduce quantization error.

  Allocation of levels is not perfect. It does not take into account the number of existing levels for a nonterminal.  For example, if a nonterminal produces all of its terminals with uniform probability and that group is allocated more than one level, the extra levels are not reallocated to other nonterminals.

- `quantizeriters`

  The number of iterations with random starts used by the quantizer to search for an optimal solution.  The chosen solution will be the iteration with lowest mean-squared error.  Lower values can make quantization run more quickly, but might decrease accuracy.  The default value is 1000.  

- `cutoffmin`*, `cutoffmax`*

  These specify a probability cutoff range. The framework will generate a lookup table that covers all guesses up to a given probability, and beyond that you won't have guess numbers.  You will probably want to set cutoffmin and cutoffmax to the same value in most cases.

  If `cutoffmin != cutoffmax`, then the framework will generate tables using a for loop with conditions: `i = cutoffmin; i >= cutoffmax; i /= 10.0`.  This lets you run the calculator with some safe cutoff and let it keep running until some cutoff exhausts disk space, at which point you still have results for the previous cutoff (The `iterate_experiments` script will monitor free disk space on the current disk and stop when the disk is over 95% full.)  Iterating cutoffs in this way is an automated yet slow process.

  **NOTE**: There is no way to ensure that a given cutoff will reach any specific number of guesses. The probability distribution of passwords is different for each configuration of the framework, so there is no way to know what the right cutoff is a priori to reach a particular guess number threshold. We have found that `'1e-14'` is a good starting point for the cutoff and should get you into the trillions of guesses.

- `training_configuration`*

  Input files can be in .txt and .gz format.  See [here](#training-data).  There are three types of sources that can be defined: structure sources, untokenized terminal sources, and tokenized terminal sources.  For each source type, inputs are filtered and then combined into a single wordfreq file before applying a tokenizer.

  - `structure sources`*

    Structure sources specify training data files from which structures will be learned. The value for each source type (`structure sources`, `tokenized terminal sources`, and `untokenized terminal sources`) is an array of hashes, where each hash has the following possible keys.

    - `name`*

    - `filename`*

    - `filter`

      A filter which takes a wordfreq file and produces wordfreq output to STDOUT for passwords that are accepted by the filter.
      
    - `ID`

      Sets a [tag](#tags) that will appear in the output. The tag should be different for different sources.

    - `use_remainder`

      This switch is used in conjunction with the `save_remainder` switch on a test source.  If both switches are set, a source can be used for testing and training and the framework will automatically partition the source into training and holdout sets.  Training and test sources are matched by `filename`.

    - `weight`

      The weight applied to the input source.  The default value is 1.  Weights can be specified in two different ways.

      Numeric weights act as multipliers on the input source, so a weight of 100 means that each password from the source is treated as if it represented 100 passwords.  

      String weights ending in 'x', like `10x`, cause a source to be treated as if its cumulative frequency were that many times the cumulative frequency of all previous sources in the array.  For example, if source[0] has one million passwords and source[1] has one thousand. Assigning the weight `10x` to source[1] would cause it to be treated as if it had ten million passwords.  This is equivalent to a numeric weight of 10000.  String weights should be used when balancing weights between different sources.  Another example: if three sources have dissimilar numbers of passwords but should be treated equally, we can assign them weights of 1, '1x', and '0.5x' respectively.

  - `tokenized terminal sources`

    You specify tokenized terminal sources in the same manner as structure sources.  These sources will be used for learning terminals, after being tokenized using `tokenize_tts_method`.

  - `untokenized terminal sources`

    You specify untokenized terminal sources in the same manner as structure sources.

- `testing_configuration`

  Test files must be in plaintext (see [here](#test-data).)  The `testing_conguration` hash contains a single `sources` key which must be an array of hashes representing test sources.

  - `sources`

    Source hashes can have the following keys.

    - `name`*

    - `filename`*

    - `description`

    - `filter`

      A filter which takes a plaintext file and outputs lines accepted by the filter.  Note that the input format to this filter will depend on the format of your test source. If the source is in three-column format, the filter should take a three-column, tab-delimited input. Otherwise, if `convert_from_single_column` is used, the filter should take a file that has one password per line.

    - `count`

      A number of passwords to randomly select from the input source to use as a test set.  Passwords are selected after filtering.

    - `convert_from_single_column`

      Set this to 1 to specify that the input is not in the standard three-column format.  It will be converted to three columns:

      ```
      no_user    source_name    password
      ```

      This is handy if you want to combine lookup results from multiple experiments with a single set of test data.  Since the second column is populated with the value from the `name` field of this source, you can dynamically set distinct names for each experiment.

    - `save_remainder`

      This switch is used in conjunction with the `use_remainder` switch on a training source.  If both switches are set, a source can be used for testing and training and the framework will automatically partition the source into training and holdout sets.  Training and test sources are matched by *filename*.

    

### Writing filters and tokenizers

Filters must be perl scripts; they are run with perl "$scriptname".
Typically, filters are used to require that training
data matches the conditions that the testing passwords were created
under.

Tokenizers are similar to filters except that they must accept input
in gzipped word-frequency format, and they must be executable. 
Additionally, if a tokenizer is used for tokenizing terminals, it also
takes the tokenized structures file as an argument.  This is in case
the tokenizer wants to use the structures to parse out corresponding 
terminals.  A tokenized string is identified by the use of the ASCII
1 character `\x01` which separates terminals within the password field of
a file in word-frequency format.


### Lookup result format

The result of the framework is a file of lookup results where each
line represents a password in the test set.  The framework will also
produce a "totalcounts" file which contains the number of guesses
that were evaluated in a given experiment.

Lines in the lookup-results file are tab-separated with six fields:

```
no_userv4    RockYou    tweeten    0x1.8f3a15984f7a2p-25    fontana    1661663    RYG
```


The six fields are:

1. User identifier

  If the test set was not in three-column format, this field is set to "no_user", otherwise it uses the identifier from the original source.

2. Policy name / test source identifier

  If the test set was not in three-column format, this field is set to the source name, otherwise it uses the policy name from the original source.

3. Password

4. Probability of the password based on the PCFG

  If the PCFG can produce the string in multiple ways, i.e., the string can be parsed according to the PCFG in multiple ways, then this is the parse with highest probability. The probability is expressed in hexadecimal, floating-point notation. If the password cannot be parsed, this field is set to -1.

5. Pattern identifier

  This string represents the *pattern* to which the password belongs.  A pattern is a bundle of guesses with the same probability and the same structure.  This field is used for debugging and is not required to use the guess calculator framework.

6. Guess number
 
  This is the password's rank in the list of all strings produced by the PCFG, when sorted in probability order.

  If the guess number could not be determined (either because the password can not be produced by the PCFG, or its guess number is beyond the end of the lookup table), then this field is negative. It identifies why the guess number was not found by summing various codes as follows:

  - -2:  password beyond guess number cutoff (end of lookup table)
  - -4:  no matching structure in PCFG
  - -8:  no matching terminal in PCFG
  - -32: terminal could not be produced by PCFG

7. Training source tags

  This is a concatenated list of training source tags as defined in the 'ID' field of your config file.


## 3 Advanced usage instructions

The best way to learn how to use the framework is to try out the example conditions.  Looking up guess numbers for an experiment involves four steps: creating the training and test corpora, learning a PCFG from the corpora, using the PCFG to build a lookup table, and looking up guess numbers for passwords in the test corpus.  The first step is accomplished by creating a configuration file and filters for an experiment, and running `create_training_and_test_corpora.pl`.  The second through fourth steps are all run in sequence by the `iterate_experiments.pl` script.

If you wish to perform these steps individually, there are individual scripts and programs for doing so.  This can be useful for debugging.  The operation of these steps is outlined below.

#### Step 2: Learning a PCFG

Run `iterate_experiments` with the `-o` switch to learn a PCFG which is saved in a `calculator.zip` file in a directory named after the condition. For example, if your condition is named 'minibasic6', after running this step there will be a file named `calculator.zip` in a `minibasic6-*` directory.  The zip file contains the learned PCFG and tools for building the lookup table and getting guess numbers.  You can unzip the file and run `make` to run these tools individually.  This creates a *calculator* directory.

If you run `iterate_experiments` with the `-k` switch, it will retain a calculator directory and lookup tables from the last run.  You will need a calculator directory to perform the following steps.

#### Step 3: Building a lookup table

In a calculator directory, run `parallel_gentable.pl` to create a 'raw' table.  This script will shard the `nonTerminalRules.txt` file of the grammar and generate patterns for each shard in parallel.  You can also run the `GeneratePatterns` binary directly to produce the same output, though this will run on a single core and will take significantly longer. To create a lookup table, you must sort the raw table and compute a prefix sum for each line using the `sortedcountaggregator` binary.  The complete process can be performed as follows:

```
$ ./parallel_gentable.pl -c <cutoff> -n <cores to use in parallel> > rawtable
$ sort -gr rawtable > sortedtable
$ ./sortedcountaggregator < sortedtable > lookuptable
```

#### Step 4: Looking up guess numbers

In a calculator directory with a lookup table, run `parallel_lookup.pl` to shard an input file of passwords, and run lookups on each shard in parallel.  You can also run the `LookupGuessNumbers` binary directly, but this will take significantly longer.

```
$ ./parallel_lookup.pl -n <cores to use> -L lookuptable -I <password file> > lookupresults
```

The above step might be useful if you have already built and saved a lookup table (using the `-k` switch to `iterate_experiments`) and want to look up additional passwords without waiting for the lookup table to be rebuilt.


## 4 Generating strings directly for online attack modeling

In online attacks, the adversary only gets to make a small number of guesses per account.  For these guesses to be most effective, we want passwords with the highest frequency in our training data to be tried first.  This doesn't always happen with a PCFG, because passwords are tokenized, i.e., broken into pieces, and a context-free grammar doesn't care about how the pieces were originally put together.

For more accurate online guesses, you will want to adopt two strategies:

1. When tokenizing, enable 'hybrid structures' with:

  ```
  'tokenize_structures_arguments' => "-d 0.01"
  ```

  For each password in the input data set, this will add both tokenized and untokenized structures to the PCFG.  With untokenized structures, passwords from the input data set will be tried verbatim, i.e., not broken into pieces.

2. Generate strings instead of patterns using accurate probabilities.

  The `parallel_gentable.pl` script can be run manually with the `-A` switch to generate a list of strings whose probability is above a particular cutoff and then sort this list by probability.  *Accurate* probabilities means that we sum over all possible tokenizations of a string.  Since strings take up a lot more space than patterns, you will want to choose a very low cutoff. WARNING: `parallel_gentable.pl` does not check available disk space while it runs so it could fill up the disk if run unchecked.


The full procedure for generating strings is as follows:

- Set the condition configuration to duplicate structures using the `tokenize_structures_arguments` parameter

- Learn the PCFG as described in Step 2 of [Advanced usage instructions](#advanced-usage-instructions)

- Unzip the produced zip file to some directory and run `make`.

- Run `parallel_gentable.pl` with the `-A` switch:

  ```
  $ ./parallel_gentable.pl -c <high cutoff> -A > strings.txt
  ```


If you have a target number of guesses in mind, it is recommended to first generate a lookup table (using the `-k` switch of `iterate_experiments`) using a cutoff of 1e-12 or so.  Then, you can inspect the table to find an appropriate probability to use when generating strings, e.g., the minimum probability associated with a guess number of 1.2 times your target number of guesses. The 1.2 fudge factor is meant to compensate for the fact that the lookup table can include duplicates, while generated strings with accurate probabilities will not.


## 5 Emulating the guessing model from Weir 2009

It is possible to emulate results from [Weir 2009] using the included filters and tokenizer.  The key components of this approach are tokenizing solely by character-class, i.e., no mixed-class terminals, and uniform string frequencies.

There are two steps to reproducing this behavior.

1. Use the included character-class tokenizer by including:
  ```
  my $structures_tokenizer = "./character_class_tokenizer.py"
  ```
  in your configuration.  Structure sources are automatically used as a source of terminals, since having nonterminals without associated terminals results in a malformed PCFG.

2. Turn on uniformized strings in your configuration file.
  ```
  'uniformize_strings' => 1,
  ```
  
  This will tell the framework to rewrite the terminals corpus so all purely alphabetic terminals have the same apparent frequency.

To include additional digit, symbol, or strings sources, simply add them to the configuration as `'untokenized terminal sources'` with `filter_digsym.pl` and/or `filter_strings.pl` to extract single-character-class blocks.  You can also add them as `'tokenized terminal sources'` using the character_class_tokenizer and a "remainder" filter as explained [above](#conducting-a-policy-experiment).

You should also keep the "hybrid structures" and "unseen terminals" learning parameters turned off, if you are copying another configuration.

[Weir 2009]
    Weir, M., Aggarwal, S., Medeiros, B. d., and Glodek, B. Password cracking using probabilistic context-free grammars. In Proceedings of the 2009 IEEE Symposium on Security and Privacy, IEEE (2009), 391–405.


## 6 Emulating the guessing model from Weir 2010

A simple utility is provided for emulating results from [Weir 2010] as well.  This does not use the guess calculator framework and can only be used when the desired number of guesses is small.

The process starts with a single training file of source passwords.  The training set is sorted by descending frequency and deduplicated.  Guess number 1 is assigned to the most popular password, and guess numbers are assigned sequentially down this list.  The maximum guess number is the number of distinct passwords in the training set.

The `simpleguess.pl` script in the `scripts/` directory facilitates this.  It is run with:
```
$ cat <training file> | perl scripts/simpleguess.pl <test file> [policy name] > lookupresults.output 2> totalcounts.output
```
or
```
$ ./process_wordfreq.py <training file> | perl scripts/simpleguess.pl <test file> [policy name] > lookupresults.output 2> totalcounts.output
```
The test file must be in plaintext format with one password per line (single-column).  The training input must be in plaintext wordfreq format.  As shown above, you can use the `process_wordfreq.py` utility to convert your training file into this format. The `simpleguess.pl` script returns a nonzero errorlevel if unsuccessful.

Stdout is sent a file in [lookup result format](#lookup-result-format).  You can modify the `policy name` field of the lookup result file with an optional second parameter. Stderr is sent a totalcounts file.  Using these formats means that its outputs are compatible with the plotting tools in `PlotResults.R`. For example, after running the above you can plot the output with:
```
$ Rscript PlotResults.R makeplot output.pdf
```



[Weir 2010]
    Weir, M., Aggarwal, S., Collins, M., and Stern, H. Testing metrics for password creation policies by attacking large sets of revealed passwords. Proceedings of the 17th ACM Conference on Computer and Communications Security, ACM (2010), 162–175.


## 6 Acknowledgements

Thanks to William Melicher for contributing to this section.
