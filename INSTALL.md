# Requirements

The framework was developed for a single machine running Ubuntu 12.04 with 64 cores and 256 GB of RAM.  It can run on smaller machines, within limits.  It was developed and tested using the following tools and packages.  These tools, or newer versions, must be installed:

- The OS must be a 64-bit Linux

- The maximum number of open files must be set higher than is typical for most systems.
`ulimit -n` needs to report at least 16K, but the value below is recommended.
To increase the number of allowed open files in a shell, try `sudo sh -c "ulimit -n 600000 && exec su $LOGNAME"`.
In Ubuntu, you can permanently increase this limit by editing `/etc/security/limits.conf` and adding the line:

    ```
    soft   nofile   600000
    ```

- The maximum number of open memory-mapped files might also need to be set higher than the default for most systems.
`cat /proc/sys/vm/max_map_count` should be at least 200K.
To increase the number of allowed memory-mapped files, try `sudo sysctl -w vm.max_map_count=600000`.  Unlike ulimit, you do not need to open a new shell for the change to take effect.
In Ubuntu, you can permanently increase this limit by editing `/etc/sysctl.conf` and adding the line:

    ```
    vm.max_map_count=600000
    ```

- Python 2.7.3

- Perl 5.14.2

- g++ 4.6.3 (many c++0x features are used, so an updated gcc version is critical)
  - GMP libraries (libgmp-dev in Ubuntu)
  - POSIX file and mmap libraries

- R 3.1.1.  
R scripts will attempt to download missing packages automatically from CMU's CRAN mirror.

- An updated version of Bash and GNU utilities
  - GNU sort 8.13
  - GNU shuf 8.13
  - Bash 4.2.25

- If linguistic tokenization is desired, you can use the included `google_ngram_tokenizer.py` and `google_ngram_tokenizer_wrapper.sh` scripts.  These scripts require:
  - A *token* file - a file with three, tab-separated columns, like so:
  
      ```
      a       a       9083738245
      aa      a a     5172914
      aa      aa      30555185
      aaa     a a a   3339106
      aaa     a aa    105285
      aaa     aa a    94511
      aaa     aaa     9430615
      ```
  - A *keys* file - a trie of the unique keys in the leftmost column of the token file, serialized into [marisa trie](https://pypi.python.org/pypi/marisa-trie) format.
  - a build of the marisa_trie Cython library
    - `pip install marisa-trie`  
    OR  
    with Python dev tools installed (python2.7-dev in Ubuntu):

        ```
        wget https://pypi.python.org/packages/source/m/marisa-trie/marisa-trie-0.6.tar.gz
        tar xvf marisa-trie-0.6.tar.gz
        cd marisa-trie-0.6
        python setup.py build
        ```     
    Modify the paths `TOKENFILENAME` and `KEYSFILENAME` in `google_ngram_tokenizer.py` to point at the locations of the token and keys files.  Then copy the `marisa-trie.so` file from `build/lib-.../marisa_trie.so` to the guess calculator framework root directory (the same directory that contains `google_ngram_tokenizer.py`)

    See the FAQ for detailed instructions on producing token and keys files from the [Google Web Corpus](http://googleresearch.blogspot.com/2006/08/all-our-n-gram-are-belong-to-you.html).

The machine must have at least 1 GB of RAM per available core.  If it does not, limit the number of cores used per run
by specifying the `-n` argument to `iterate_experiments`.  Run the script with no arguments to see usage information.

