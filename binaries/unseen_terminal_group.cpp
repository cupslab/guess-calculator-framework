// unseen_terminal_group.cpp - a class for handling "unseen" terminal groups, 
//   which fill in the gap in training data that would be occupied by unseen
//   terminals
//
// Use of this source code is governed by the GPLv2 license that can be found
//   in the LICENSE file.
//
// Version 0.1
// Author: Saranga Komanduri
//   Based on code originally written and published by Matt Weir under the
//   GPLv2 license.
// 
// Modified: Fri Jul 18 10:50:19 2014
// 
// See header file for additional information

// Includes not covered in header file
#include <cstdlib>
#include <cstdint>
#include <climits>
#include "grammar_tools.h"
#include "bit_array.h"

#include "unseen_terminal_group.h"

// Non-literal static class members cannot be initialized in the class body
const std::string UnseenTerminalGroup::kGeneratorSymbols = 
  "`~!@#$%^&*()-_=+[{]}\\|;:\'\",<.>/\? ";

UnseenTerminalGroup::UnseenTerminalGroup(const char *terminal_data, 
                                         const double probability,
                                         const std::string& generator_mask,
                                         const std::string& out_representation,
                                         const size_t terminal_data_size)
    : TerminalGroup(terminal_data,
                    0,
                    out_representation),
      terminal_data_size_(terminal_data_size),
      generator_mask_(generator_mask),
      total_probability_mass_(probability) {
  // Iterate over the terminal data to get the number of seen terminals
  mpz_init(terminals_size_);

  // To work quickly with terminals, there are a number of class variables that
  // need to be initialized
  initCharacterLookups();

  // Check if modifications are needed and record
  if (out_representation_.find('U') != std::string::npos)
    out_matching_needed_ = true;
  else
    out_matching_needed_ = false;

  // Now process terminal_data and set first_string_
  if (!processSeenTerminals()) {
    fprintf(stderr, "Failed processing seen terminals in UnseenTerminalGroup"
                    " constructor!\n");
    exit(EXIT_FAILURE);    
  }
}


// Called from the constructor, this routine iterates over the seen terminals
// in terminal_data_ and determines the count of seen terminals 
// (seen_terminals_size_), count of unseen terminals (terminals_size_),
// total number of generatable terminals (total_terminals_),
// probability of each individual terminal (probability_),
// and the value of the first unseen string.
// 
// Return false on any error
//
bool UnseenTerminalGroup::processSeenTerminals() {
  // Iterate over terminal_data_ until we reach a blank line
  const char *data_position = terminal_data_;
  size_t bytes_remaining = terminal_data_size_;
  mpz_t seen_terminals_size, seen_terminals_cant_be_generated;
  mpz_init_set_ui(seen_terminals_size, 0);  
  mpz_init_set_ui(seen_terminals_cant_be_generated, 0);  
  while (bytes_remaining > 0) {
    // Read the current line
    char read_buffer[1024];
    unsigned int bytes_read;
    grammartools::ReadLineFromCharArray(data_position, bytes_remaining,
                                        read_buffer, bytes_read);
    // If current line is blank, expect unseen terminals next and terminate
    if (bytes_read == 1) {  // Blank line = just the newline character was read
      break;
    }

    // Else parse the line
    std::string terminal, source_ids;
    double probability;
    grammartools::ParseNonterminalLine(read_buffer, terminal,
                                       probability, source_ids);

    // Check if this terminal can actually be produced by the generator mask
    if (canGenerateTerminal(terminal))
      mpz_add_ui(seen_terminals_size, seen_terminals_size, 1);
    else
      mpz_add_ui(seen_terminals_cant_be_generated, 
                 seen_terminals_cant_be_generated, 1);

    // Move counters forward
    data_position += bytes_read;
    bytes_remaining -= bytes_read;
  }  // end while (bytes_remaining > 0)

  // Set total_terminals_ and terminals_size_
  initTotalTerminals();
  if (mpz_cmp(seen_terminals_size, total_terminals_) >= 0) {
    char *sts_string = mpz_get_str(NULL, 10, seen_terminals_size);
    char *tt_string  = mpz_get_str(NULL, 10, total_terminals_);
    fprintf(stderr, "Unexpected error!\n"
                    "seen_terminals_size exceeds total_terminals_ found!\n"
                    "seen_terminals_size: %s\n"
                    "total_terminals_: %s\n",
                    sts_string, tt_string);
    return false;
  }
  mpz_sub(terminals_size_, total_terminals_, seen_terminals_size);
  probability_ = total_probability_mass_ / mpz_get_d(terminals_size_);
  // Cleanup intermediate BigInts
  mpz_clear(seen_terminals_size);
  mpz_clear(seen_terminals_cant_be_generated);


  // Finally, we need to determine the value of the first unseen string.
  // We use the findUnseenTerminals method to make a pass through all seen
  // terminals and mark them in a BitArray.
  // After one pass, we check for the smallest index *not* found in the vector.
  // If the vector is full, we move to the next region of the terminal
  // space and perform another pass.  This continues until some unseen string
  // has been found.
  mpz_t region_start;
  mpz_init_set_ui(region_start, 0);
  bool first_open_index_found = false;
  bool space_traversed = false;
  if (static_bitarray == nullptr) {
    // fprintf(stderr, "Initializing static BitArray object...");
    static_bitarray = new BitArray(kTerminalSearchRegionSize);    
    // fprintf(stderr, "done!\n");
  }
  if (ba_in_use) {
    fprintf(stderr, "BitArray seems to be in use!\n");
    exit(EXIT_FAILURE);
  }
  ba_in_use = true;
  BitArray *found_terminals = static_bitarray;
  while (!first_open_index_found && !space_traversed) {
    findUnseenTerminals(region_start, kTerminalSearchRegionSize, found_terminals);

    // Check if this iteration took us to the end of the region
    mpz_t region_end;
    mpz_init(region_end);
    mpz_add_ui(region_end, region_start, kTerminalSearchRegionSize);
    if (mpz_cmp(total_terminals_, region_end) < 0) {
      space_traversed = true;
    }

    // Check BitArray for an unseen terminal
    unsigned long int open_terminal = found_terminals->findNextOpenSpace(0);
    if (open_terminal < found_terminals->getSize()) {
      // Found an unseen terminal!
      mpz_t open_terminal_index;
      mpz_init(open_terminal_index);
      mpz_add_ui(open_terminal_index, region_start, open_terminal);

      // Set first string using the open terminal index
      // Use c_str() to ensure a deep copy
      first_string_ = generateTerminal(open_terminal_index).c_str();

      // Set flag so we don't loop anymore
      first_open_index_found = true;
      mpz_clear(open_terminal_index);
    }

    // Move region forward
    mpz_set(region_start, region_end);
    mpz_clear(region_end);
  }  // end while (!first_open_index_found && !space_traversed)  
  // delete found_terminals;
  ba_in_use = false;  // free the lock
  mpz_clear(region_start);

  if (!first_open_index_found) {
    // If here, we traversed the whole space and didn't find an open spot
    fprintf(stderr, "No first unseen terminal found after traversing whole "
                    "space in UnseenTerminalGroup with out_representation_: "
                    "%s and generator mask: %s!\n",
                    out_representation_.c_str(), generator_mask_.c_str());
    return false;
  }
  return true;
}




// Initialize character lookup arrays -- these arrays allow for quick
// lookups of character types rather than checking for values in an ASCII
// range or searching through the kGeneratorSymbols string
void UnseenTerminalGroup::initCharacterLookups() {
  // Initialize arrays *_char_to_int_ to false (-1) and set values later
  for (int i = 0; i < 256; ++i) {
    l_char_to_int_[i] = -1;
    d_char_to_int_[i] = -1;
    s_char_to_int_[i] = -1;
    l_int_to_char_[i] = 0;
    d_int_to_char_[i] = 0;
  }

  // Now set index values, e.g., for an L, 'a' is index 0, 'b' is index 1, etc.
  for (int i = 'a'; i <= 'z'; ++i) {
    l_char_to_int_[i] = i - 'a';
    l_int_to_char_[i - 'a'] = i;
  }

  for (int i = '0'; i <= '9'; ++i) {
    d_char_to_int_[i] = i - '0';  
    d_int_to_char_[i - '0'] = i;
  }

  for (int i = 0; i < kGeneratorSymbols.size(); ++i)
    s_char_to_int_[ kGeneratorSymbols[i] ] = i;
}


// Private initialization function -- count the number of terminals that can
// be produced by the generator mask using a simple product
//
// Die if generator mask contains unexpected characters
//
void UnseenTerminalGroup::initTotalTerminals() {
  mpz_init(total_terminals_);
  mpz_set_ui(total_terminals_, 1);
  for (unsigned int i = 0; i < generator_mask_.size(); ++i) {
    unsigned int characters_produced;
    switch (generator_mask_[i]) {
      case 'L':
        characters_produced = 26;
        break;
      case 'D':
        characters_produced = 10;
        break;
      case 'S':
        characters_produced = kGeneratorSymbols.size();
        break;
      default:
        fprintf(stderr, "generator_mask_: %s contains unexpected characters "
                        "in UnseenTerminalGroup with out_representation_: %s!\n",
                        generator_mask_.c_str(), out_representation_.c_str());
        exit(EXIT_FAILURE);
    }
    mpz_mul_ui(total_terminals_, total_terminals_, characters_produced);
  }
}


// Given a terminal, determine if it can be produced *by the generator mask*
bool UnseenTerminalGroup::canGenerateTerminal(const std::string& terminal) const {
  if (terminal.size() != generator_mask_.size())
    return false;

  // Iterate over the generator mask and check each character of the terminal
  for (unsigned int i = 0; i < generator_mask_.size(); ++i) {
    switch (generator_mask_[i]) {
      case 'L':
        if (l_char_to_int_[ terminal[i] ] == -1)
          return false;
        break;
      case 'D':
        if (d_char_to_int_[ terminal[i] ] == -1)
          return false;
        break;
      case 'S':
        if (s_char_to_int_[ terminal[i] ] == -1)
          return false;
        break;
      default:
        fprintf(stderr, "generator_mask_: %s contains unexpected characters "
                        "in UnseenTerminalGroup with out_representation_: %s!\n",
                        generator_mask_.c_str(), out_representation_.c_str());
        exit(EXIT_FAILURE);
    }
  }

  return true;
}


// Given a terminal that can be generated, return its index
// NOTE: assumes that canGenerateTerminal has already been called!  If this
// terminal cannot be generated, the return value is indeterminate.
//
// The general algorithm is that for converting a mixed-radix number to base 10.
// The bases for each position of the terminal are given by the number of
// possible characters indicated by the character of the generator mask.
// 
// region_end provides an optional stopping criteria.  It can be very costly
// to determine a terminalIndex because it requires multiplication of BigInts,
// so if region_end is specified and the index is found to be above it, we stop
// and return the current index.  The index returned in this case will be
// above region_end, but its value should be ignored!
//
void UnseenTerminalGroup::terminalIndex(mpz_t result,
                                        const std::string& terminal, 
                                        mpz_t region_end /*= NULL*/) const {
  mpz_init_set_ui(result, 0);

  // Iterate over the generator mask and check each character of the terminal
  // Since we assume canGenerateTerminal has already been called, we deduce
  // that terminal and generator_mask_ are the same size
  for (int i = generator_mask_.size() - 1; i >= 0; --i) {
    unsigned int character_base;
    int character_index;
    switch (generator_mask_[i]) {
      case 'L':
        character_base = 26;
        character_index = l_char_to_int_[ terminal[i] ];
        break;
      case 'D':
        character_base = 10;
        character_index = d_char_to_int_[ terminal[i] ];
        break;
      case 'S':
        character_base = kGeneratorSymbols.size();
        character_index = s_char_to_int_[ terminal[i] ];
        break;
      default:
        fprintf(stderr, "generator_mask_: %s contains unexpected characters "
                        "in terminalIndex with out_representation_: %s!\n",
                        generator_mask_.c_str(), out_representation_.c_str());
        exit(EXIT_FAILURE);
    }
    if (character_index < 0) {
      fprintf(stderr, "character_index for character i: %d in generator_mask_"
                      ": %s is -1 indicating that this character: %c in "
                      "terminal: %s cannot be generated!\n"
                      "  In terminalIndex with out_representation_: %s!\n",
                      i, generator_mask_.c_str(), terminal[i],
                      terminal.c_str(), out_representation_.c_str());
      exit(EXIT_FAILURE);      
    }
    // Use the formula from: http://stackoverflow.com/a/759319
    mpz_mul_ui(result, result, character_base);
    mpz_add_ui(result, result, character_index);

    // If the current index is outside the region, stop here
    if (region_end != NULL) {
      if (mpz_cmp(result, region_end) > 0) {
        return;
      }
    }
  }
}


// Given a terminal index, use the generator mask and lookup arrays to compose
// a terminal and return it.  The general algorithm is based on conversion
// from a base-10 terminal_index to a mixed radix number.
//
// Note: the for loop here must iterate in the *opposite* direction of the for
// loop in terminalIndex.
//
// Note: This function will *destroy* the value of terminal_index.
//
std::string UnseenTerminalGroup::generateTerminal(mpz_t terminal_index) const {
  // String to store the result
  std::string generated_terminal = "";

  for (unsigned int i = 0; i < generator_mask_.size(); ++i) {
    // First use the mask symbol to get the character base and store it
    unsigned int character_base;
    switch (generator_mask_[i]) {
      case 'L':
        character_base = 26;
        break;
      case 'D':
        character_base = 10;
        break;
      case 'S':
        character_base = kGeneratorSymbols.size();
        break;
      default:
        fprintf(stderr, "generator_mask_: %s contains unexpected characters "
                        "in terminalIndex with out_representation_: %s!\n",
                        generator_mask_.c_str(), out_representation_.c_str());
        exit(EXIT_FAILURE);
    }
    // value = index % base
    unsigned long int character_value = mpz_fdiv_ui(terminal_index, character_base);

    // Now use the residual value to determine the character of the terminal
    // and add it to the end.
    char character;
    switch (generator_mask_[i]) {
      case 'L':
        character = l_int_to_char_[character_value];
        break;
      case 'D':
        character = d_int_to_char_[character_value];
        break;
      case 'S':
        character = kGeneratorSymbols[character_value];
        break;
      default:
        fprintf(stderr, "generator_mask_: %s contains unexpected characters "
                        "in terminalIndex with out_representation_: %s!\n",
                        generator_mask_.c_str(), out_representation_.c_str());
        exit(EXIT_FAILURE);
    }
    generated_terminal.push_back(character);

    // index = floor(index / base)
    mpz_fdiv_q_ui(terminal_index, terminal_index, character_base);
  }

  if (out_matching_needed_)
    matchOutRepresentation(generated_terminal);

  return generated_terminal;
}


// Helper function -- given an index to the start of a region in terminal space,
// a size for that region, and a BitArray, modify the BitArray so that elements
// are marked true for seen terminals, and false otherwise.
//
void UnseenTerminalGroup::findUnseenTerminals(
    mpz_t region_start, unsigned long int region_size, BitArray *found_terminals) const {

  // Determine true region size if region_start + region_size is greater than
  // total_terminals_
  unsigned long int true_region_size = region_size;
  mpz_t region_end;
  mpz_init(region_end);
  mpz_add_ui(region_end, region_start, region_size - 1);
  // total_terminals_ is a count, but region* are zero-indexed values, so
  // region_end should be at most total_terminals_ - 1, so if total_terminals_
  // is equal to region_end, we still need to shrink the region
  if (mpz_cmp(total_terminals_, region_end) <= 0) {
    mpz_sub_ui(region_end, total_terminals_, 1);
    // Get true region size = region_end - region_start + 1
    mpz_t true_size;
    mpz_init(true_size);
    mpz_sub(true_size, region_end, region_start);
    mpz_add_ui(true_size, true_size, 1);
    true_region_size = mpz_get_ui(true_size);
    mpz_clear(true_size);
  }
  // Set the region to false
  found_terminals->clear(true_region_size);

  // Pass over all seen terminals, convert them to lexicographic indices
  // in base-10, e.g., aaaaa = 0, aaaab = 1, etc., and mark the found
  // indices in a boolean vector of fixed size, e.g., from 0 to
  // region_size - 1.
  //
  // To speed determination of whether a given seen terminal is in our region
  // we use the region_end argument of terminalIndex (see below).
  const char *data_position = terminal_data_;
  size_t bytes_remaining = terminal_data_size_;
  while (bytes_remaining > 0) {
    // Read the current line
    char read_buffer[1024];
    unsigned int bytes_read;
    grammartools::ReadLineFromCharArray(data_position, bytes_remaining,
                                        read_buffer, bytes_read);
    if (bytes_read == 1) {
      break;
    }
    std::string terminal, source_ids;
    double probability;
    grammartools::ParseNonterminalLine(read_buffer, terminal,
                                       probability, source_ids);

    // If this terminal can be generated, get its index and store in
    // the BitArray if the index of the terminal is within our region
    // of consideration
    if (canGenerateTerminal(terminal)) {
      mpz_t terminal_index;
      terminalIndex(terminal_index, terminal, region_end);
      if (mpz_cmp(terminal_index, region_end) <= 0 &&
          mpz_cmp(region_start, terminal_index) <= 0) {
        // Adjust terminal index based on the region offset
        mpz_sub(terminal_index, terminal_index, region_start);
        unsigned long int bitarray_index = mpz_get_ui(terminal_index);
        found_terminals->markIndex(bitarray_index);
      }
      // Clear the allocated BigInt before exiting
      mpz_clear(terminal_index);
    }
    // Move counters forward
    data_position += bytes_read;
    bytes_remaining -= bytes_read;
  }  // end while (bytes_remaining > 0)
  mpz_clear(region_end);
  
  return;
}


// Simple getter function for first string
std::string UnseenTerminalGroup::getFirstString() const {
  return first_string_;
}


// Return a LookupData struct with relevant fields set for the given terminal
//
// source_id is set to "UNSEEN"
//
// Set lookup_data->index to -1 if no match, to match expected output of
// indexInTerminalGroup, which calls this function to perform the lookup.
//
// The operation of this function is fairly straightforward if you've seen
// the other methods of this class.  Use terminalIndex on the input string,
// and then subtract the number of seen terminals with a lower index.
//
LookupData* UnseenTerminalGroup::lookup(const std::string& terminal) const {
  LookupData *lookup_data = new LookupData;

  if (canGenerateTerminal(terminal))
    terminalIndex(lookup_data->index, terminal);
  else {
    lookup_data->parse_status = kTerminalNotFound | kTerminalCantBeGenerated;
    lookup_data->probability = -1;    
    mpz_init(lookup_data->index);
    mpz_set_si(lookup_data->index, -1);
    return lookup_data;
  }

  const char *data_position = terminal_data_;
  size_t bytes_remaining = terminal_data_size_;
  mpz_t lower_count;
  mpz_init_set_ui(lower_count, 0);
  while (bytes_remaining > 0) {
    // Read the current line
    char read_buffer[1024];
    unsigned int bytes_read;
    grammartools::ReadLineFromCharArray(data_position, bytes_remaining,
                                        read_buffer, bytes_read);
    // If current line is blank, expect unseen terminals next and terminate
    if (bytes_read == 1) {  // Blank line = just the newline character was read
      break;
    }
    // Else parse the line
    std::string read_terminal, source_ids;
    double probability;
    grammartools::ParseNonterminalLine(read_buffer, read_terminal,
                                       probability, source_ids);

    // Check if this terminal can actually be produced by the generator mask
    if (canGenerateTerminal(read_terminal)) {
      // Check if the index of this terminal is less than our index
      mpz_t terminal_index;
      terminalIndex(terminal_index, read_terminal);
      int compare_result = mpz_cmp(terminal_index, lookup_data->index);
      if (compare_result < 0) {
        mpz_add_ui(lower_count, lower_count, 1);
      } else if (compare_result == 0) {
        // Our input string matches a seen terminal, so return -1, but perform
        // a sanity check to make sure things are working properly.
        if (terminal != read_terminal) {
          fprintf(stderr, "string: %s has same index as terminal: %s"
                          "in indexInTerminalGroup with out_representation_: %s"
                          " but strings are not equal!\n",
                          terminal.c_str(), read_terminal.c_str(),
                          out_representation_.c_str());
          exit(EXIT_FAILURE);
        }
        mpz_clear(terminal_index);
        lookup_data->parse_status = kTerminalNotFound | kTerminalCollision;
        lookup_data->probability = -1;    
        mpz_set_si(lookup_data->index, -1);
        mpz_clear(lower_count);
        return lookup_data;
      }      
      mpz_clear(terminal_index);
    }
    // Move counters forward
    data_position += bytes_read;
    bytes_remaining -= bytes_read;
  }  // end while (bytes_remaining > 0)

  lookup_data->parse_status = kCanParse;
  lookup_data->probability = probability_;    
  mpz_sub(lookup_data->index, lookup_data->index, lower_count);
  mpz_clear(lower_count);
  // Set source id
  lookup_data->source_ids.insert("UNSEEN");
  return lookup_data;
}


// Return the "index" of the given string in the unseen terminals 
// (negative result if no match)
//
// This function simply calls lookup, and then returns just the index
// 
void UnseenTerminalGroup::indexInTerminalGroup(mpz_t result,
                                               const std::string& teststring) const {
  LookupData *lookup_data = lookup(teststring);
  mpz_init(result);
  mpz_set(result, lookup_data->index);

  // Cleanup
  mpz_clear(lookup_data->index);
  delete lookup_data;
}


// Create new iterator for this group
UnseenTerminalGroup::UnseenTerminalGroupStringIterator* 
    UnseenTerminalGroup::getStringIterator() const {
  UnseenTerminalGroupStringIterator *iterator =
    new UnseenTerminalGroupStringIterator(this);

  return iterator;
}


// Constructor for iterator -- unlike the SeenTerminalGroup iterator we will
// maintain (and manage) a bit array that holds the next unseen terminals
// to generate.
//
// *** Class logic ***
// After construction and after execution of each method:
// - if isEnd is true, then found_terminals, current_bitarray_index,
//     and current_string_ are all indeterminate, otherwise:
//   - found_terminals will be a valid BitArray for the region starting at
//       region_start_
//   - current_bitarray_index_ will point at the location in the BitArray
//       that corresponds to current_string_
// - isEnd is true iff region_start is greater than total_terminals_, i.e.,
//     we have moved outside the range of generatable terminals.
//
UnseenTerminalGroup::UnseenTerminalGroupStringIterator::
    UnseenTerminalGroupStringIterator(const UnseenTerminalGroup* const parent)
    : parent_(parent) {
  mpz_init_set_ui(region_start_, 0);
  found_terminals_ = new BitArray(kTerminalSearchRegionSize);
  parent_->findUnseenTerminals(
    region_start_, kTerminalSearchRegionSize, found_terminals_);
  current_bitarray_index_ = -1;  
  increment();
}

UnseenTerminalGroup::UnseenTerminalGroupStringIterator::
    ~UnseenTerminalGroupStringIterator() {
  mpz_clear(region_start_);
  delete found_terminals_;
}

// Increment the iterator by one and set current_string_ to the new value
// Return false if we are past the end
bool UnseenTerminalGroup::UnseenTerminalGroupStringIterator::
    increment() {      
  if (!isEnd()) {
    unsigned long int new_index = 
      found_terminals_->findNextOpenSpace(static_cast<unsigned long int>(current_bitarray_index_ + 1));

    // Recurse if no unseen terminals left in the current region
    if (new_index >= found_terminals_->getSize()) {
      // Move ahead to a new region
      mpz_add_ui(region_start_, region_start_, kTerminalSearchRegionSize);
      // Check if past the end
      if (mpz_cmp(region_start_, parent_->total_terminals_) >= 0) {
        return false;
      }
      parent_->findUnseenTerminals(
        region_start_, kTerminalSearchRegionSize, found_terminals_);
      current_bitarray_index_ = -1;
      return increment();
    }

    if (new_index > LONG_MAX) {
        fprintf(stderr, "new_index too large in UnseenTerminalGroupStringIterator::increment!"
                        " Maybe the BitArray size is too large?\n");
        exit(EXIT_FAILURE);
    }
    // This is an unsigned to signed conversion, hence the above check
    current_bitarray_index_ = new_index;
    // Found an unseen terminal!
    mpz_t open_terminal_index;
    mpz_init(open_terminal_index);
    mpz_add_ui(open_terminal_index, region_start_, new_index);

    // Set first string using the open terminal index
    // Use c_str() to ensure a deep copy
    current_string_ = (parent_->generateTerminal(open_terminal_index)).c_str();

    mpz_clear(open_terminal_index);
    return true;
  } else {
    return false;
  }
}

// Set the iterator back to the beginning
void UnseenTerminalGroup::UnseenTerminalGroupStringIterator::
    restart() {
  mpz_set_ui(region_start_, 0);
  current_bitarray_index_ = -1;
  parent_->findUnseenTerminals(
    region_start_, kTerminalSearchRegionSize, found_terminals_);
  increment();  
}

// Simple check
bool UnseenTerminalGroup::UnseenTerminalGroupStringIterator::
    isEnd() const {
  // total_terminals_ is a count, but region_start is zero-indexed, so if
  // total_terminals_ == region_start_ then we are past the end.
  return (mpz_cmp(region_start_, parent_->total_terminals_) >= 0);
}


// Simple getter
std::string UnseenTerminalGroup::UnseenTerminalGroupStringIterator::
    getCurrentString() const {
  return current_string_;
}

