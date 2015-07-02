#!/usr/bin/env bash
# This script will take in input password sets and a dictionary and will produce a zip file that contains all the data needed to run an experiment.
# Version 0.5
#
# 0.3 - Adding error catching
# 0.4 - Pass on switch for generating unseen terminals
# 0.5 - Switch from strings and digsym to terminals

# Check that all arguments are there
if [ $# -ne 7 ]
then
	echo "This script will take in input password sets and a dictionary and will produce a jar file that contains all the data needed to run an experiment."
	echo
	echo "Usage: `basename $0` ARGUMENT-LIST"
	echo "ARGUMENT-LIST:"
	echo "  1. <directory location of binaries, e.g. modpcfg_manager, process.py>"
	echo "  2. <file containing passwords from which structures will be learned>"
	echo "  3. <file containing passwords from which terminals will be learned>"
	echo "  4. <string containing the root name for the output file>"
	echo "  5. <number of total quantizer levels for quantizer> (use 500 if you are not sure)"
	echo "  6. <number of iterations for quantizer> (use 1000 if you are not sure)"
	echo "  7. <generate unseen terminals? (0 for no, 1 for yes)"
	exit 1
fi

function error_exit
{
	echo "$1" 1>&2
	exit 1
}

# Process argument 7 -- option to generate unseen terminals
GENUNSEENARG=""
if [ "$7" -eq "1" ]; then
	GENUNSEENARG="-u"
fi

# Process argument 1
BINARY_DIR=`cd "$1"; pwd`

# Process argument 2
echo "Processing argument 2"
ARG2=$(readlink -f "$2")
echo "ARG2=$ARG2"
CURDIR=`pwd`
DIR1="structuretraining${RANDOM}${RANDOM}${RANDOM}"
mkdir "$DIR1"
cd "$DIR1"
mkdir grammar
mkdir grammar/terminalRules
"$BINARY_DIR/process.py" -vt "$ARG2"
if [[ $? -ne  0 ]]; then
	error_exit "Error processing structure corpus.  Aborting."
fi
cd - > /dev/null
echo

# Process argument 3
echo "Processing argument 3"
ARG3=$(readlink -f "$3")
echo "ARG3=$ARG3"
DIR2="terminaltraining${RANDOM}${RANDOM}${RANDOM}"
mkdir "$DIR2"
cd "$DIR2"
mkdir grammar
mkdir grammar/terminalRules
"$BINARY_DIR/process.py" -vg $GENUNSEENARG "$ARG3"
if [[ $? -ne  0 ]]; then
	error_exit "Error processing digit/symbol corpus.  Aborting."
fi
cd - > /dev/null
echo

# Collect files into one directory
echo "Collecting files"
DIR4="combined${RANDOM}${RANDOM}${RANDOM}"
mkdir "$DIR4"
mkdir "$DIR4/grammar"
mkdir "$DIR4/grammar/terminalRules"
cp "$DIR1/grammar/nonterminalRules.txt" "$DIR4/grammar/nonterminalRules.txt"
cp -r "$DIR2/grammar/terminalRules" "$DIR4/grammar"
cp -r "$BINARY_DIR/." "$DIR4"

echo "Applying quantizer"
mv "$DIR4/grammar/terminalRules" "$DIR4/grammar/terminalRulesold"
cd "$DIR4"
echo "ARG4=$4"
echo "ARG5=$5"
echo "ARG6=$6"
Rscript --vanilla ../scripts/ApplyQuantizer.R $5 $6
if [[ $? -ne  0 ]]; then
	error_exit "Error running quantizer.  Aborting."
fi
cd - > /dev/null

# Save combined directory to zip file
cd "$DIR4"
zip -r "../$4.zip" "."
if [[ $? -ne  0 ]]; then
	error_exit "Error creating zip file."
fi
cd - > /dev/null


echo "Removing temporary directories"
rm -rf "$DIR1"
rm -rf "$DIR2"
rm -rf "$DIR4"
echo

