// bit_array.h - a class for encapsulating bit array functions used for
//   unseen terminal generation
//
// Use of this source code is governed by the GPLv2 license that can be found
//   in the LICENSE file.
//
// Version 0.2
// Author: Saranga Komanduri
//   Based on code originally written and published by Matt Weir under the
//   GPLv2 license.
// 
// Modified: Sat Nov 22 13:18:49 2014
// 

// This class is initially planned to use a simple STL bit vector, but in
//   future versions it should take advantage of __builtin_clz and other low
//   level operations (memset?) to optimize its operations.
//

#ifndef BIT_ARRAY_H__
#define BIT_ARRAY_H__

#include <vector>
#include <cstddef>
#include <cassert>

class BitArray {
public:
  BitArray(unsigned long int size) {
    bitarray_ = new std::vector<bool>(size);    
    maxsize_ = size;
    size_ = size;
  }
  ~BitArray() {
    delete bitarray_;
  }

  unsigned long int getSize() { return size_; }

  void clear(unsigned long int size) {
    assert(size <= maxsize_);
    bitarray_->assign(size, false); 
    size_ = size;
  }

  void markIndex(unsigned long int index) { (*bitarray_)[index] = true; }

  // Return the index of the first unset space in the bitarray, starting
  // at startindex.  Returns getSize() if not found.
  unsigned long int findNextOpenSpace(unsigned long int startindex = 0) {
    unsigned long int i;
    for (i = startindex; i < size_; ++i)
      if (!(*bitarray_)[i])
        break;
    return i;
  }

private:
  unsigned long int maxsize_;
  unsigned long int size_;
  std::vector<bool> *bitarray_;
};

// HACK - create a reusable object here so we don't have to constantly
// allocate and deallocate BitArrays
static BitArray *static_bitarray = nullptr;
static bool ba_in_use = false;  // lock variable


#endif // BIT_ARRAY_H__