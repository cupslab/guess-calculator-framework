# FAQ

## Unicode

The framework currently ignores Unicode and operates purely at the byte level.
Unicode characters will be read and reproduced as strings of symbols, rather than strings of letters.
This is mostly due to the large number of languages used in the framework and our lack of confidence that these languages will all handle Unicode the same way.



## Common problems

If you have a problem, check the generated logs. Here are some
solutions to problems that have occurred in the past.

#### Results from example conditions don't match the reference results

Results will vary slightly on each run due to randomness
introduced by the quantizer.  You can use the `-b` switch
to iterate_experiments to eliminate this randomness.  You
can also plot your results using the PlotResults.R script
and compare your plot to the example.


#### Can't install R packages
    
You may not have permissions for system installation. Specify a
user library location with: 

```
$ mkdir ~/.R_libs 
$ export R_LIBS="~/.R_libs"
```

#### R packages were built for a previous version of R

Try the following command. Make sure to change lib.loc to the
same place as R_LIBS if you don't have permissions in the system
location. 

``` 
> update.packages(ask=FALSE, 
                  checkBuilt=TRUE, 
                  repos="http://lib.stat.cmu.edu/R/CRAN/", 
                  dependencies=TRUE, 
                  lib.loc="/home/billy/.R_libs")
```

You may have to install packages manually because the dependency
handling for updated packages in R does not work very well.


#### makeTables failed

Check that your filters create valid files. The command `perl
create_training_and_test_corpora.pl` should create .gz files in
the work directory. If any of these files is empty, it is possible
that a filter is not working correctly. You can use the `zcat` command
to print the output of gzipped files.

    
#### Can't find files in config file

Check that your config file points to the correct files. Paths in
the config file should be relative to the calling directory. Also
check that the config file is in the correct location. 

#### Too many open files

You need to increase your system's open file limit as described in INSTALL.md.

## Creating token and keys files

This section explains how to create token and keys files from the [Google Web Corpus](http://googleresearch.blogspot.com/2006/08/all-our-n-gram-are-belong-to-you.html) for use with the `google_ngram_tokenizer` scripts.

#### Format

The Google Web Corpus consists of files in the following format:
```
a b c   467
a b d   615128
```
Tokens are separated by spaces, and the frequency is separated by a tab. Before using the corpus, combine the `1gms`, `2gms`,`3gms`,`4gms`, and `5gms` split files into single files.

Notes:
* We ignore any lines containing non-alphabetic tokens. This includes the `</S>` and `</UNK>` tokens.  
* We ignore case in the corpus.


#### Token file

Producing a token file requires the following steps:

1. Filter out lines with non-alphabetic tokens

1. Downcase tokens

1. Create a new column with spaces removed between tokens.  For example,

  ```
  a b c    467
  ```

  is converted to

  ```
  abc      a b c     467
  ```

1. Sort the resulting file

1. Since we downcased tokens, multiple lines can contain the same tokens.  Collapse lines with the same first two columns into a single line by summing frequencies.

The above steps can be performed with the following commands:
  
```
$ cat ../1gms/1gms ../2gms/2gms ../3gms/3gms ../4gms/4gms ../5gms/5gms | 
perl -CSD -nE'print if /^[A-Za-z ]+\t/' | 
perl -CSD -nE'print lc' | 
perl -CSD -nE'my $oldstr = $_; $_ =~ /^([^\t]+)\t/; my $mystr = $1; $mystr =~ s/[ ]//g; print "$mystr\t$oldstr"' |
LC_COLLATE=C sort -T . | 
perl -CSD -nE'@f = $_ =~ m/^([^\t]+\t[^\t]+)\t([0-9]+)/;
  die "Input is malformed!" if scalar(@f) != 2; 
  if ($. == 1) { $prevkey = $f[0]; $c = 0; }
  if ($prevkey ne $f[0]) {
    print $prevkey, "\t", $c, "\n"; $c = $f[1];
  } else {
    $c += $f[1];
  }
  $prevkey = $f[0];
  END {   
    print $prevkey, "\t", $c, "\n";
}' > alphagrams.txt
```

#### Keys file

Building the keys file requires the [marisa trie](https://pypi.python.org/pypi/marisa-trie) library. See INSTALL.md for instructions on installing it.

1. Create a file of unique keys from the token file

    ```
    cat alphagrams.txt | cut -d$'\t' -f1 | uniq > alphagramkeys.txt
    ```

1. Produce the keys file using Python.  In this example, we write the file `alphagramkeys.marisa`.
    ```
    $ python
    >>> import marisa_trie
    >>> lines = (line.rstrip('\n') for line in open("alphagramkeys.txt"))
    >>> trie = marisa_trie.Trie(lines)  # this statement requires 90 GB of memory
    >>> trie.save("alphagramkeys.marisa")
    ```
