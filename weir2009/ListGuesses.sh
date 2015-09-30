#!/usr/bin/env bash
# 

# This is a simple shell script for generating explicit guesses from Weir's 2009
# PCFG. The code was originally downloaded from
# https://sites.google.com/site/reusablesec/Home/password-cracking-tools/probablistic_cracker/
# Given a training list of passwords, we do the following:
# 
# 1. Make directories to hold the grammar (digits, grammar, and special)
# 2. Run process.py on the training list to learn the grammar
# 3. Filter the training list to extract alphabetic strings
# 4. Run pcfg_manager to generate guesses using the filtered training list
#
# pcfg_manager will use the alphabetic strings from the given dictionary as
# terminals for letters in the structures learned in step 2.  It learns digits
# and symbols natively.
#
# Output is to a file called "pcfg_output.txt"
# Note that guesses are generated in sorted order by descending probability
#
# Written by Saranga Komanduri
#
set -e

TRAININGFILE=$1

if [ $# -ne 1 ]
then
  echo "No training file specified"
  exit 1
fi

make

# Make grammar directories
rm -rf digits grammar special
mkdir digits
mkdir grammar
mkdir special

echo "Learning grammar"
./process.py "$TRAININGFILE"

echo "Filtering training file"
FILTEREDFILE=$(mktemp)
egrep -o '[a-zA-Z]+' "$TRAININGFILE" > "$FILTEREDFILE"

echo "Enumerating guesses"
trap ctrl_c INT
function ctrl_c() {
  echo "CTRL-C detected.  Killing pcfg_manager."
  pkill -9 pcfg_manager
  rm "$FILTEREDFILE"
}
./pcfg_manager -dname0 "$FILTEREDFILE" -removeSpecial -removeDigits > pcfg_output.txt &
sleep 10s
time bash killpcfg.sh

rm "$FILTEREDFILE"