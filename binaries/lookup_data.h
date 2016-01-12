// lookup_data.h - Simple data structure passed between objects of the guess
//   calculator framework that contains various pieces of information about
//   a string lookup
//
// Use of this source code is governed by the GPLv2 license that can be found
//   in the LICENSE file.
//
// Version 0.1
// Author: Saranga Komanduri
//   Based on code originally written and published by Matt Weir under the
//   GPLv2 license.
//
// Modified: Wed Jul 30 09:10:12 2014
//

// parse_status overrides other fields, i.e., if parse_status is a
// non-parseable code, such as kTerminalNotFound, ignore the index,
// probability, and other fields.
//

#ifndef LOOKUP_DATA_H__
#define LOOKUP_DATA_H__

#include <gmp.h>
#include <cstdint>
#include <unordered_set>

// ParseStatus is an enum of bit flags
enum ParseStatus {
    kCanParse = 1 << 0,
    kBeyondCutoff = 1 << 1,
    kStructureNotFound = 1 << 2,
    kTerminalNotFound = 1 << 3,
    kTerminalCollision = 1 << 4,
    kTerminalCantBeGenerated = 1 << 5,
    kUnexpectedFailure = 1 << 6
};

// Allow parse statuses to be combined via bit operations
inline ParseStatus operator|(ParseStatus a, ParseStatus b) {
  return static_cast<ParseStatus>(static_cast<int>(a) | static_cast<int>(b));
}

struct LookupData {
    ParseStatus parse_status;
    double probability;
    mpz_t index;
    std::unordered_set<std::string> source_ids;
    std::string first_string_of_pattern;
};

struct TerminalLookupData : LookupData {
    uint64_t terminal_group_index;
};


#endif // LOOKUP_DATA_H__
