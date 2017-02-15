// unseen_terminal_group.h - a class for handling "unseen" terminal groups, 
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
// Modified: Thu May 29 14:52:57 2014
// 

// This file implements the UnseenTerminalGroup class that inherits from the
// TerminalGroup abstract class.  Unlike SeenTerminalGroup, this class
// is based on the terminals that are NOT found in the the memory-mapped
// file it uses as a data source.  These terminals are generated based on a
// mask stored in each object.
//
// Note that uppercasing is handled by calling matchOutRepresentation only
// after the terminal is generated.  The generator mask can only create
// lowercase strings.
//
// Assume the generator mask is of the form: {L,S,D}*
// The total number of terminals that can be produced by this mask is the
// product of the total number of characters that each can produce:
// L = 26, D = 10, S = 33 (printable characters on a standard keyboard)
//
// Technically, the seen terminals can produce extra symbol strings, because
// they can include byte sequences that create unicode, but I ignore these in
// symbol generation.
//
// It is assumed that if no unseen terminals exist, that this constructor will
// never be called, so exit with failure if the number of seen terminals exceeds
// the total number that can be generated.  This could occur for symbols, so
// this should be handled by upstream calls.
//

#ifndef UNSEEN_TERMINAL_GROUP_H__
#define UNSEEN_TERMINAL_GROUP_H__

#include <string>
#include <gmp.h>

#include "terminal_group.h"
#include "bit_array.h"

class UnseenTerminalGroup : public TerminalGroup {
public:
  UnseenTerminalGroup(const char *terminal_data, 
                      const double probability,
                      const std::string& generator_mask,
                      const std::string& out_representation,                      
                      const size_t terminal_data_size);
  ~UnseenTerminalGroup() { 
    // Not much to do
    mpz_clear(total_terminals_);
    mpz_clear(terminals_size_);
  }

  // Return a LookupData struct with relevant fields set for the given terminal
  LookupData* lookup(const char *terminal) const;

  // Return the "index" of the given string in the terminal group (-1 if no match)
  void indexInTerminalGroup(mpz_t result, const char *teststring) const;

  // Return the "first" string of the terminal (used for string representation)
  std::string getFirstString() const;


  class UnseenTerminalGroupStringIterator : public TerminalGroupStringIterator {
  public:
    // Forward declare virtual methods from TerminalGroupStringIterator:
    UnseenTerminalGroupStringIterator(const UnseenTerminalGroup* const parent);
    ~UnseenTerminalGroupStringIterator();
    void restart();
    bool increment();
    bool isEnd() const;
    std::string getCurrentString() const;

  private:
    const UnseenTerminalGroup* const parent_;
    mpz_t region_start_;
    BitArray *found_terminals_;
    long int current_bitarray_index_;
    std::string current_string_;
  };

  UnseenTerminalGroupStringIterator* getStringIterator() const;

private:
  static const std::string kGeneratorSymbols;  // This is assigned in the .cpp file
  static const unsigned int kTerminalSearchRegionSize = 0x40000000;

  // Helper initialization functions for the constructor
  void initCharacterLookups();
  bool processSeenTerminals();
  void initTotalTerminals();

  // Given a string from terminal_data_, determine if it can be produced
  // by the generator mask.
  //
  // Since terminal_data_ is all lowercase, this function should only be given
  // downcased strings.
  bool canGenerateTerminal(const char *terminal) const;

  // Given a terminal that can be generated, return its index, with an optional
  // stopping criteria.  Note that this will not return the same value as the
  // indexInTerminalGroup public method (though it is called from there).  The
  // returned index is in "terminal space" or the total space of terminals
  // produced by the generator mask.  This includes those seen terminals that
  // can be generated.  In contrast, indexInTerminalGroup will subtract out
  // seen terminals from the returned index.
  void terminalIndex(mpz_t result, 
                     const char *terminal, 
                     mpz_t region_end = NULL) const;
  
  // Given an index in terminal space, generate a terminal
  // The result is matched to out_representation_, so if this includes
  // uppercase, canGenerateTerminal will return false if given the result.
  // This is expected behavior.
  std::string generateTerminal(mpz_t terminal_index) const;
  // Given a starting index and region size in terminal space, return a BitArray
  // with seen terminals marked.  Used in generating unseen terminals.
  void findUnseenTerminals(mpz_t region_start, 
                           unsigned long int region_size,
                           BitArray *found_terminals) const;


  const size_t terminal_data_size_;
  const std::string generator_mask_;
  mpz_t total_terminals_;
  double total_probability_mass_;

  // Lookup arrays built for speed -- initialized by initCharacterLookups
  int l_char_to_int_[256];
  int d_char_to_int_[256];
  int s_char_to_int_[256];
  char l_int_to_char_[256];
  char d_int_to_char_[256];
    // A version for symbols is not needed, since we can look up characters
    // in GENERATED_SYMBOLS_

  bool out_matching_needed_;
};


#endif // UNSEEN_TERMINAL_GROUP_H__
