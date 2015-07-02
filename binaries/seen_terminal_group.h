// seen_terminal_group.h - a class for handling "seen" terminal groups, 
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

// This file implements the SeenTerminalGroup class that inherits from the
//   TerminalGroup abstract class.  It uses a memory-mapped file as a data
//   source but it's operations should be restricted to a single section
//   of the file pertaining to this particular group.

#ifndef SEEN_TERMINAL_GROUP_H__
#define SEEN_TERMINAL_GROUP_H__

#include <string>
#include <gmp.h>

#include "terminal_group.h"

class SeenTerminalGroup : public TerminalGroup {
public:
  SeenTerminalGroup(const char *const terminal_data, 
                    const double probability,
                    const mpz_t terminals_size,
                    const std::string& out_representation,
                    const char *const group_data_start,
                    const size_t group_data_size)
      : TerminalGroup(terminal_data,
                      probability,
                      out_representation),
        group_data_start_(group_data_start),
        group_data_size_(group_data_size) {
    mpz_init_set(terminals_size_, terminals_size);
    loadFirstString();
  }
  // Destructor
  ~SeenTerminalGroup() {
    mpz_clear(terminals_size_);
  };

  // Return a LookupData struct with relevant fields set for the given terminal
  LookupData* lookup(const std::string& terminal) const;

  // Return the "index" of the given string in the terminal group (-1 if no match)
  void indexInTerminalGroup(mpz_t result, const std::string& teststring) const;

  // Return the "first" string of the terminal (used for string representation)
  std::string getFirstString() const;

  class SeenTerminalGroupStringIterator : public TerminalGroupStringIterator {
  public:
    // Forward declare virtual methods from TerminalGroupStringIterator:
    SeenTerminalGroupStringIterator(const SeenTerminalGroup* const parent);
    ~SeenTerminalGroupStringIterator();
    void restart();
    bool increment();
    bool isEnd() const;
    std::string getCurrentString() const;

  private:
    const SeenTerminalGroup* const parent_;
    const char* current_group_position_;
    size_t group_data_size_;
    size_t bytes_remaining_;
    std::string current_string_;
  };

  SeenTerminalGroupStringIterator* getStringIterator() const;

private:
  // Read from the terminal_data to set the first string
  void loadFirstString();

  const char *const group_data_start_;
  const size_t group_data_size_;
  bool out_matching_needed_;
};


#endif // SEEN_TERMINAL_GROUP_H__