// nonterminal.h - a class for single nonterminals and their associated
//   production rules.
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

// This file implements the Nonterminal class as used in the restricted
// PCFG of the guess calculator framework.  Currently, nonterminals only
// produce terminals, so we can just store a list of terminal "groups."
// A terminal group is simply a collection of terminals that share a
// probability and correspond to a collection of RHSs for production
// rules of this nonterminal.
//
// Because the number of terminals can be arbitrarily large, terminals
// are kept on disk and accessed via a memory-mapped file.  All terminals
// for a given nonterminal are stored in the same file, and this class
// will handle the memory map creation/destruction and pass it to
// TerminalGroup objects.
//
// The name of this file is the string representation of the nonterminal
// and it will be located in the terminals_folder, given in the
// LoadNonterminal initialization routine. The file must be sorted by
// descending probability, and terminal groups are identified by
// contiguous terminals that share a probability. A blank line will separate
// seen terminal groups from unseen groups in the file, with all seen
// terminals in the first part of the file, and unseen groups in the second
// part.
//

#ifndef NONTERMINAL_H__
#define NONTERMINAL_H__

#include <gmp.h>
#include <string>
#include <cstdint>

#include "gcfmacros.h"
#include "terminal_group.h"
#include "lookup_data.h"

class Nonterminal {
public:
  // As with other objects in the PCFG, true initialization is deferred
  Nonterminal():
    terminal_groups_(NULL),
    terminal_groups_size_(0),
    terminal_data_(NULL),
    representation_(""),
    terminal_representation_("") {}
  ~Nonterminal();

  // Initializer routine - memory maps the terminal data file and processes
  // the terminal groups.
  bool loadNonterminal(const std::string& representation,
                       const std::string& terminals_folder);

  // By convention, mpz_t types are not returned, but are passed by reference
  // See http://stackoverflow.com/a/13396028
  void countStrings(mpz_t result) const;
  uint64_t countTerminalGroups() const;

  // See if the given terminal can be produced by this nonterminal and return
  // a LookupData struct with relevant fields set
  TerminalLookupData* lookup(const std::string& inputstring) const;
  bool canProduceTerminal(const std::string& inputstring) const;

  // Routines for getting values from the terminal groups
  std::string getFirstStringOfGroup(uint64_t group_index) const;
  double getProbabilityOfGroup(uint64_t group_index) const;
  void countStringsOfGroup(mpz_t result, uint64_t group_index) const;
  TerminalGroup::TerminalGroupStringIterator* getStringIteratorForGroup(
      uint64_t group_index) const;

  // getter methods
  const std::string& getRepresentation() const;

private:
  // Part of the delayed initialization process -- called from LoadNonterminal
  bool initializeTerminalGroups();

  // Nonterminals are implemented as a collection of terminal group object
  // pointers along with terminal data stored in a memory-mapped file
  TerminalGroup* *terminal_groups_;
  uint64_t terminal_groups_size_;
  // The memory mapping is found at terminal_data_ and we also store the size
  char* terminal_data_;
  size_t terminal_data_size_;

  std::string representation_;
  // Terminals don't use uppercase, so terminal_representation_ won't
  // contain "U" (it is replaced with "L")
  std::string terminal_representation_;  

  // Disable copy and assignment
  DISALLOW_COPY_AND_ASSIGN(Nonterminal);
};


#endif // NONTERMINAL_H__
