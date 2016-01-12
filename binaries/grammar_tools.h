// grammar_tools.h - a collection of miscellaneous, low-level functions
//   used when reading and parsing the grammar *on-disk* into higher-level
//   objects
//
// Use of this source code is governed by the GPLv2 license that can be found
//   in the LICENSE file.
//
// Version 0.1
// Author: Saranga Komanduri
//   Based on code originally written and published by Matt Weir under the
//   GPLv2 license.
//
// Modified: Fri May 30 18:35:30 2014
//
// Functions are declared within the grammar_tools namespace

#ifndef GRAMMAR_TOOLS_H__
#define GRAMMAR_TOOLS_H__

#include <cstdio>
#include <string>
#include <cstdint>
#include <unordered_set>

namespace grammartools {

// Given a file pointer, count the number of lines (including the current line)
// up to the next blank line.  E.g., if the current line is blank, 1 is returned.
//
// Note: The file pointer should return to its position before the function
// was evaluated when the function returns.
//
// Return -1 if no blank line is found.
int CountLinesToNextBlankLine(FILE *fileptr);

// Skip the header line(s) in a structure file
// Note: This modifies the file pointer that was passed in.
//
// Return number of header lines on success, -1 on failure
int SkipStructuresHeader(FILE *fileptr);

// Read a line from the structures file and parse out the fields, which
// are returned in the out-parameters.
//
// Return true on success, output the offending line to stderr on failure
bool ReadStructureLine(FILE *fileptr,
                       std::string& structure,
                       double& probability,
                       std::string& source_ids);

// Remove the \x01 character from the input string
std::string StripBreakCharacterFromTerminal(const std::string& inputstring);


// Given a source pointer of size source_length, place one line from the
// source into the destination and provide the number of bytes read.
//
// Assumes line is less than 1024 bytes long (source_length can be any size)
//
// Returns true on success
bool ReadLineFromCharArray(const char *source, size_t source_length,
                           char *destination, unsigned int &bytes_read);

// Read a line from a source buffer, taken from a nonterminal file, and parse
// out the fields that are returned in the out-parameters.
//
// NOTE: This function uses strtok which destroys that source buffer.
//
// Return true on success, output the offending line to stderr on failure
bool ParseNonterminalLine(char *source,
                          std::string& terminal,
                          double& probability,
                          std::string& source_ids);

// Given a source pointer of size source_length, count the number of terminal
// groups in the data and place the result in an out parameter.
//
// Return true on success, return false on any failure to parse
bool CountTerminalGroupsInText(const char *source, size_t source_length,
                               uint64_t& number_of_groups);

// Given a source pointer of size source_length, check if the current line is
// at the end of a terminal group.  The boolean is returned in an out parameter.
//
// The current line is at the end of a terminal group if:
// - The next line has a different probability from the current line
// - The next line is blank
// - There is a current line but no next line (end of source)
//
// If the current line is blank, this is not considered the end of a group.
//
// Return true on success, return false on any failure to parse
bool IsEndOfTerminalGroup(const char *source, size_t source_length,
                          bool& end_of_group);

// Given a string of source_ids, which is a comma-separated list of values,
// parse out the values and add them to the given std::unordered set
//
// Return true on success, false on any failure
bool AddSourceIDsFromString(const std::string& source_ids,
                            std::unordered_set<std::string>& source_list);

} // namespace grammartools

#endif // GRAMMAR_TOOLS_H__
