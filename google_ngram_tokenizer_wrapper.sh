#!/usr/bin/env bash
# This is a simple wrapper script that calls the google_ngram_tokenizer and
# shows how any program could be adapted to act as a tokenizer by using a
# wrapper script.
#
# The script supports two situations: one where '-d x' is specified before
# the filename to tokenize and one where the filename is the first argument.
#
# Version 0.2
#
function error_exit
{
  echo "$1" 1>&2
  exit 1
}


if [ -e "$1" ]; then
  # Pass file to ngram tokenizer                               
  zcat "$1" | ./google_ngram_tokenizer.py 28 
else
  if [ -e "$3" ]; then
    # Pass file to ngram tokenizer along with any other arguments
    zcat "$3" | ./google_ngram_tokenizer.py $1 $2 28 
  else
    error_exit "Did not find existing file in first or third arguments! Arguments given: $* "
  fi
fi  

if [[ $? -ne  0 ]]; then
  error_exit "Error running tokenizer!"
fi


