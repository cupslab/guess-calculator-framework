// mixed_radix_number.cpp - a class that implements a mixed-radix number with
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
// Modified: Thu Jun  5 12:41:57 2014
//
// See header file for additional information

// Includes not covered in header file

#include "mixed_radix_number.h"

// Constructor
// Assign bases to the positions_ array in order from 0 to size - 1
MixedRadixNumber::MixedRadixNumber(const uint64_t *radices,
                                   const unsigned int size) {
  positions_ = new DigitWithRadix[size];
  size_ = size;
  for (unsigned int i = 0; i < size_; ++i) {
    positions_[i].base = radices[i];
  }
  clear();
}

// Deep copy function -- Make a new object and copy this object's properties
// to it.
MixedRadixNumber* MixedRadixNumber::deepCopy() const {
  // Get radices into a single array
  uint64_t *copy_of_radices = new uint64_t[size_];
  for (unsigned int i = 0; i < size_; ++i) {
    copy_of_radices[i] = positions_[i].base;
  }

  // Make new object with these radices
  MixedRadixNumber* copy = new MixedRadixNumber(copy_of_radices, size_);
  delete[] copy_of_radices;

  // Now just need to copy positions
  for (unsigned int i = 0; i < size_; ++i) {
    copy->positions_[i].digit = positions_[i].digit;
  }

  return copy;
}


// Destructor
MixedRadixNumber::~MixedRadixNumber() {
  delete[] positions_;
}


// Set all digits to zero
void MixedRadixNumber::clear() {
  for (unsigned int i = 0; i < size_; ++i) {
    positions_[i].digit = 0;
  }
}


// Use idea from Knuth's Algorithm M to increment and catch overflow
// j was the index variable used by Knuth
bool MixedRadixNumber::increment() {
  long int j = size_ - 1;
  // Proactively reduce values to account for overflow
  while (j >= 0 &&
         (positions_[j].digit >= (positions_[j].base - 1))) {
    positions_[j].digit = 0;
    --j;
  }
  if (j < 0) {
    // This number was about to overflow!
    return false;
  } else {
    ++(positions_[j].digit);
    return true;
  }
}


// Return the next number whose probability might be higher than the current
// number, given assumptions about how positions in this number map to terminal
// probabilities.
bool MixedRadixNumber::intelligentSkip() {
  // Skipping is fairly simple: from right to left, max out the digits up to
  // and including the first non-zero digit.  This gets us to the last number
  // whose probability we know is lower than the current number.  Then, call
  // the increment routine to get to the next number.
  long int j = size_ - 1;
  bool reached_nonzero = false;
  while (!reached_nonzero && j >= 0) {
    if (positions_[j].digit != 0)
      reached_nonzero = true;
    positions_[j].digit = positions_[j].base - 1;
    --j;
  }

  return increment();
}


// Simple getter function
uint64_t MixedRadixNumber::getPlace(unsigned int place) const {
  return positions_[place].digit;
}

// Function for setting places manually - returns true on success
bool MixedRadixNumber::setPlace(unsigned int place, uint64_t value) {
  if (place < size_ && value < positions_[place].base) {
      positions_[place].digit = value;
      return true;
  }
  return false;
}
