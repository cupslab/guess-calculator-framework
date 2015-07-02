# Overview

This condition is designed to provide a small-scale test of the guess calculator framework, using a minimum six-character password policy.


## Inputs

The inputs to the condition come from the leaked RockYou and Yahoo! Voices password sets.  Inputs are tokenized using Weir's method which tokenizes based solely on character class.

Training files contain 10000 random passwords from the RockYou and Yahoo! Voices password sets, and test files contain 100 random passwords from these sets.  Passwords were sampled so that training and test sets do not overlap.  In addition, the one million most common words in the Google Web Corpus are included with frequency information. This file has been prefiltered and processed into a frequency table using the process_wordfreq.py tool to save space.

