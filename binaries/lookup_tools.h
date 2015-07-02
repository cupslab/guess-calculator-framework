// lookup_tools.h - a collection of low-level functions used to search a
//   lookup table produced by the guess calculator
//
// Use of this source code is governed by the GPLv2 license that can be found
//   in the LICENSE file.
//
// Version 0.1
// Author: Saranga Komanduri
//   Based on code originally written and published by Matt Weir under the
//   GPLv2 license.
// 
// Modified: Thu Aug  7 12:50:12 2014
// 
// Functions are declared within the lookuptools namespace

#ifndef LOOKUP_TOOLS_H__
#define LOOKUP_TOOLS_H__

#include <fstream>
#include <string>
#include <cstdio>

#include "lookup_data.h"

namespace lookuptools {

// Given an istream containing lines of a password file in lookup-format,
// parse out the password and original line and return in out-parameters.
//
// Return false if the password field could not be found in the line.
bool ReadPasswordLineFromStream(std::ifstream& passwordFile,
                                std::string& fullline,
                                std::string& password);


// Read and parse a line from the lookup table file, passed as a FILE pointer.
// Check the read line for proper format.
//
// On failure, output the offending line to stderr and return false.
bool ReadLookupTableLine(FILE *fileptr, 
                         double& probability,
                         std::string& guess_number, 
                         std::string& pattern_string);


// Given a file pointer, use fseeko to go back in characters until we have gone
// back one line.  The goal is to make the next fgets call return a valid "line"
// which will end in a '\n'. If we RewindOneLine after an fgets call, the next
// fgets call should return the same result, i.e., we should return to the same
// position (assuming the first fgets call was at the start of a line).
//
// Note: The return value of false is only useful if you are certain that you
// should not be close to the begininng of the file when RewindOneLine is called.
// Otherwise, a returned false is harmless and can be ignored.
bool RewindOneLine(FILE *lookupFile);


// Get the last (lowest) probability from the lookup file.  We do this by
// seeking to the end of the file and then rewinding until we read the last
// probability.
//
// Die on any unexpected error.
double FindLastProbability(FILE *lookupFile);


// Perform a binary search on the lookup table file for the given probability
// key.
//
// Return a ParseStatus type to indicate the status of the search.  On success,
// return kCanParse and expect file position to be at the beginning of a block
// of matching lines, else return an error status as shown above.
//
// Die on any unexpected error, such as file operation errors.
ParseStatus BinarySearchLookupTable(FILE *lookupFile, double key);


// Given a FILE pointer to a lookup table, and two keys: a probability and
// a pattern string, return a LookupData object where parse_status represents
// the search success and index is the rank of this pattern, i.e., guess number
// as found in the lookup table.
//
// Note that unlike the way ranks are computed everywhere else in the guess
// calculator framework, the guess number is one-indexed rather than zero-indexed.
// This is because it represents a count, rather than an abstract rank or index.
//
LookupData *TableLookup(FILE *lookupFile, const double probability, 
                        const std::string& patternkey);


} // namespace lookuptools

#endif // LOOKUP_TOOLS_H__