// mixed_radix_number.h - a class that implements a mixed-radix number with
//   increment and the "intelligent skipping" algorithm used in the guess
//   calculator framework
//
// Use of this source code is governed by the GPLv2 license that can be found
//   in the LICENSE file.
//
// Version 0.1
// Author: Saranga Komanduri
//   Based on code originally written and published by Matt Weir under the
//   GPLv2 license.
//
// Modified: Thu Jun  5 12:41:51 2014
//
// Structures can be represented as a sequence of nonterminals, and each
// nonterminal produces a number of terminal groups.  To iterate over all
// productions of a structure, we can represent each nonterminal with a counter
// from 0 to (#terminal_groups - 1).  The sequence of nonterminals is then
// naturally represented with a mixed-radix number, with one position for each
// nonterminal, and the base of each position given by the number of terminal
// groups that it produces.
//
// If the counters are then organized such that 0 corresponds to terminals
// with highest probability and probabilities decrease as counters increase,
// we can implement an "intelligent skipping" algorithm to speed up traversal
// of the structure.  I believe a more optimal algorithm for this might exist,
// but for now this works reasonably well.
//
// The basic operation of the class is Increment, which is simply a +1 in
// mixed-radix arithmetic.  For the guess calculator framework, we also add an
// IntelligentSkip operation.  This finds the next number whose probability is
// not known to be less than the current number.  For example, if the current
// number is 34502, then Increment would go to 34503.  We know that 34503 will
// have a lower probability than 34502, because a higher digit in a given
// position is guaranteed to have a lower probability than a lower digit.
// IntelligentSkip would take 34502 and go to 34510.  Similarly, 34510 should
// IntelligentSkip to 34600.
//
// The mixed-radix number is organized such that each digit and its
// corresponding base is a single object.
//
#ifndef MIXED_RADIX_NUMBER_H__
#define MIXED_RADIX_NUMBER_H__

#include "gcfmacros.h"
#include <cstdint>

class MixedRadixNumber {
public:
  struct DigitWithRadix {
    uint64_t digit;
    uint64_t base;
  };

  // Construct object with given radices and all digits set to 0
  MixedRadixNumber(const uint64_t *radices, const unsigned int size);
  ~MixedRadixNumber();

  // Reset all digits to zero
  void clear();

  // The following routines return false on overflow
  bool increment();
  bool intelligentSkip();

  // Simple getter function
  uint64_t getPlace(unsigned int place) const;

  // Function for setting places manually - returns true on success
  bool setPlace(unsigned int place, uint64_t value);

  // Return a deep copy of this object
  MixedRadixNumber* deepCopy() const;

private:
  DigitWithRadix *positions_;
  unsigned int size_;

  // Disable copy and assignment
  DISALLOW_COPY_AND_ASSIGN(MixedRadixNumber);
};


#endif // MIXED_RADIX_NUMBER_H__
