// seen_terminal_group.cpp - a class for handling "seen" terminal groups, 
//   which are built on terminals seen in the training data
//
// Use of this source code is governed by the GPLv2 license that can be found
//   in the LICENSE file.
//
// Version 0.1
// Author: Saranga Komanduri
//   Based on code originally written and published by Matt Weir under the
//   GPLv2 license.
// 
// Modified: Thu May 29 14:52:57 2014
// 
// See header file for additional information

// Includes not covered in header file
#include <cstdlib>
#include <cctype>  // for toupper
#include <string.h> // strncpy

#include "grammar_tools.h"

#include "seen_terminal_group.h"

// Initialize the "first" string of the terminal and set out_matching_needed_
void SeenTerminalGroup::loadFirstString() {
  if (mpz_cmp_ui(terminals_size_, 0) < 1) {
    fprintf(stderr, "terminals_size_ is 0 in SeenTerminalGroup::loadFirstString!"
                    " out_representation_: %s\n", out_representation_.c_str());
    exit(EXIT_FAILURE);
  }

  unsigned int bytes_read;
  if (!grammartools::ReadLineFromCharArray2(group_data_start_,
                                           bytes_read)) {
    fprintf(stderr, "Failed read in SeenTerminalGroup::loadFirstString!\n");
    exit(EXIT_FAILURE);
  }    

  const char *terminal, *source_ids;
  double probability;
  if (!grammartools::ParseNonterminalLine(group_data_start_, bytes_read, &terminal,
                                          probability, &source_ids)) {
    fprintf(stderr,
      "Line could not be parsed in SeenTerminalGroup::loadFirstString!\n");
    exit(EXIT_FAILURE);
  }    

  // terminal is the first string in the terminal data, modify it to match
  // the out_representation_
  //
  // Check for a size mismatch -- we only check this here, in loadFirstString
  // and assume that this function will work for all other modifications
  if (strlen(terminal) != out_representation_.size()) {
    fprintf(stderr,
      "out_representation could not be matched in "
      "SeenTerminalGroup::loadFirstString!\n"
      "out_representation_: %s\nterminal: %s\n",
      out_representation_.c_str(), terminal);
    exit(EXIT_FAILURE);    
  }

  // Check if modifications are needed and record
  std::string terminalstr(terminal);

  if (out_representation_.find('U') != std::string::npos) {
    out_matching_needed_ = true;
    matchOutRepresentation(terminalstr);
  } else {
    out_matching_needed_ = false;
  }

  first_string_ = terminalstr;
}



// Simple getter function for first string
std::string SeenTerminalGroup::getFirstString() const {
  return first_string_;
}


// Return a LookupData struct with relevant fields set for the given terminal
//
// Set lookup_data->index to -1 if no match, to match expected output of
// indexInTerminalGroup, which calls this function to perform the lookup.
//
// NOTE: This function assumes that terminals_size_ is accurate so we can
// iterate up to terminals_size_ entries from the start of the group
// (terminal_data_) without anything horrible happening.  Otherwise, we should
// check the return values from ReadLineFromCharArray and ParseNonterminalLine
// to make sure they work correctly.
//
// Die on failure to parse source_ids
//
LookupData* SeenTerminalGroup::lookup(const char *terminal) const {
  LookupData *lookup_data = new LookupData;

  mpz_init_set_ui(lookup_data->index, 0);
  const char* current_data_position = group_data_start_;
  size_t bytes_remaining = group_data_size_;

  // Iterate over the group, looking for the input string
  while(mpz_cmp(lookup_data->index, terminals_size_) < 0) {
    unsigned int bytes_read;
    grammartools::ReadLineFromCharArray2(current_data_position,
                                        bytes_read);
    // Parse the line
    const char *read_terminal, *source_ids;
    double probability;
    // just for debug output
    char *read_buffer = (char*)calloc(bytes_read + 1, 1);
    strncpy(read_buffer, current_data_position, bytes_read);

    grammartools::ParseNonterminalLine(current_data_position, bytes_read, &read_terminal,
                                       probability, &source_ids);

    auto len = strlen(terminal);
    if ((len == strlen(read_terminal)) && (strncmp(terminal, read_terminal, len) == 0)) {
      lookup_data->parse_status = kCanParse;
      if (probability_ != probability) {
        fprintf(stderr,
          "Probability of terminal group doesn't match in line %s in "
          "SeenTerminalGroup::lookup (should be %f, found %f)!\n",
          read_buffer, probability_, probability);
        exit(EXIT_FAILURE);
      }
      lookup_data->probability = probability_;
      // index is already set correctly
      if (!grammartools::AddSourceIDsFromString(source_ids, 
                                                lookup_data->source_ids)) {
        fprintf(stderr,
          "Could not parse source ids %s in line %s in "
          "SeenTerminalGroup::lookup!\n",
          source_ids, read_buffer);
        exit(EXIT_FAILURE);
      }
      return lookup_data;
    }

    free(read_buffer);

    // Increment counters
    mpz_add_ui(lookup_data->index, lookup_data->index, 1);
    current_data_position += bytes_read;
    bytes_remaining -= bytes_read;
  }

  // If we are here, then terminal was not found
  lookup_data->parse_status = kTerminalNotFound;
  lookup_data->probability = -1;
  mpz_set_si(lookup_data->index, -1);
  return lookup_data;
}



// Return the "index" of the given string in the terminal group (-1 if no match)
//
// This function simply calls lookup, and then returns just the index
//
void SeenTerminalGroup::indexInTerminalGroup(mpz_t result, 
                                             const char *teststring) const {
  LookupData *lookup_data = lookup(teststring);
  mpz_init(result);
  mpz_set(result, lookup_data->index);

  // Cleanup
  mpz_clear(lookup_data->index);
  delete lookup_data;
}


// Create new iterator for this group
SeenTerminalGroup::SeenTerminalGroupStringIterator* 
    SeenTerminalGroup::getStringIterator() const {
  SeenTerminalGroupStringIterator *iterator =
    new SeenTerminalGroupStringIterator(this);

  return iterator;
}


SeenTerminalGroup::SeenTerminalGroupStringIterator::
    SeenTerminalGroupStringIterator(const SeenTerminalGroup* const parent)
    : parent_(parent),
      current_group_position_(parent->group_data_start_),
      bytes_remaining_(parent->group_data_size_) {
  // Use increment to read the first line and position the iterator counter
  // past the first entry
  increment();
}

SeenTerminalGroup::SeenTerminalGroupStringIterator::
    ~SeenTerminalGroupStringIterator() {
}

// Set the iterator back to the beginning
void SeenTerminalGroup::SeenTerminalGroupStringIterator::
    restart() {
  current_group_position_ = parent_->group_data_start_;
  bytes_remaining_ = parent_->group_data_size_;
  increment();
}

// Increment the iterator by one and set current_string_ to the new value
// Return false if there are no more terminals in the group (we are past the end)
bool SeenTerminalGroup::SeenTerminalGroupStringIterator::
    increment() {
  if (!isEnd()) {
    unsigned int bytes_read;
    grammartools::ReadLineFromCharArray2(current_group_position_,
                                        bytes_read);
    // Parse the line
    const char *terminal, *source_ids;
    double probability;
    grammartools::ParseNonterminalLine(current_group_position_, bytes_read, &terminal,
                                       probability, &source_ids);
    // Adjust class variables
    current_group_position_ += bytes_read;
    // Adjust bytes_remaining_ but make sure we don't overflow the unsigned in
    if (bytes_read <= bytes_remaining_)
      bytes_remaining_ -= bytes_read;
    else
      bytes_remaining_ = 0;

    std::string terminalstr(terminal);
    // Uppercase the terminal in the correct positions, if needed
    if (parent_->out_matching_needed_)
      parent_->matchOutRepresentation(terminalstr);

    current_string_ = terminalstr.c_str();
    return true;
  }
  return false;
}

// Simple check
bool SeenTerminalGroup::SeenTerminalGroupStringIterator::
    isEnd() const {
  return (bytes_remaining_ == 0);
}


// Simple getter
std::string SeenTerminalGroup::SeenTerminalGroupStringIterator::
    getCurrentString() const {
  return current_string_;
}

