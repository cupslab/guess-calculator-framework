// terminal_group.h - an abstract base class for handling terminal groups, 
//   collections of terminal strings in the guess calculator framework
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
// This file implements the TerminalGroup class as used in the restricted
// PCFG of the guess calculator framework.  A terminal group is a
// collection of terminals that share a probability and can all be
// produced the same nonterminal.  Working with these collections as a
// group greatly increases many operations in the guess calculator
// framework.
//
// This class is implemented as an abstract class because there are two
// types of terminal groups.  Groups whose terminals come from seen
// training data, and groups whose terminals were "unseen."
//
// This file also implements an iterator for the class that can be used
// to iterate over the strings produced by both types of terminal groups
// using the same interface.
//
#ifndef TERMINAL_GROUP_H__
#define TERMINAL_GROUP_H__

#include <gmp.h>
#include <string>

#include "lookup_data.h"

class TerminalGroup {
public:
  TerminalGroup(const char *const terminal_data, 
                const double probability,
                const std::string& out_representation) 
    : terminal_data_(terminal_data),
      probability_(probability),
      out_representation_(out_representation) {}

  virtual ~TerminalGroup() {}

  void countStrings(mpz_t result) { 
    mpz_init_set(result, terminals_size_); 
  }
  double getProbability() { return probability_; }
  // Return the "first" string of the terminal (used for string representation)
  virtual std::string getFirstString() const = 0;

  // Look up a terminal in this group
  // Returns a LookupData struct with relevant fields set
  virtual LookupData* lookup(const std::string& terminal) const = 0;

  // Return just the "index" of the given string in the terminal group (-1 if 
  // no match).  
  // By convention, mpz_t types are not returned, but are passed by reference.
  // See http://stackoverflow.com/a/13396028
  virtual void indexInTerminalGroup(mpz_t result, 
                                    const std::string& teststring) const = 0;

  class TerminalGroupStringIterator {
  public:
    virtual ~TerminalGroupStringIterator() {}

    // Reset the iterator to the first string
    virtual void restart() = 0;
    // Increment to next step but return false if past the end
    virtual bool increment() = 0;
    // Check if past the end
    virtual bool isEnd() const = 0;
    // Return the terminal string at the current iterator state
    virtual std::string getCurrentString() const = 0;
  };

  // Return a string iterator object
  virtual TerminalGroupStringIterator* getStringIterator() const = 0;


protected:
  // Currently, the only thing we care about is matching the up-casing characters
  // (represented by 'U' in out_representation_)
  // 
  // Assumes that terminal and out_representation_ have the same size
  void matchOutRepresentation(std::string& terminal) const {
    for (unsigned int i = 0; i < terminal.size(); ++i)
      if (out_representation_[i] == 'U')
        terminal[i] = toupper(terminal[i]);
  }


  // Both types of terminal groups will need access to terminal data
  const char *const terminal_data_;

  // All terminals in the group share a common probability
  double probability_;

  // Terminals may need to be modified (uppercased) to match some given
  // representation
  const std::string out_representation_;

  // For convenience we need to have the number of terminals covered by this
  // group available and the first string.
  mpz_t terminals_size_;
  std::string first_string_;
};




#endif // TERMINAL_GROUP_H__