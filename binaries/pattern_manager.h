// pattern_manager.h - a class for managing "groups" of nonterminals
//   in a structure, used for "pattern compaction"
//
// Use of this source code is governed by the GPLv2 license that can be found
//   in the LICENSE file.
//
// Version 0.1
// Author: Saranga Komanduri
//   Based on code originally written and published by Matt Weir under the
//   GPLv2 license.
//
// Modified: Thu Jun  5 23:42:55 2014
//
// TODO: This class is too large and unwieldy.  Some of its functionality
// should be delegated to other classes.
//
// Structures can be represented as a simple sequence of nonterminals, but
// this ignores the fact that nonterminals can repeat within a structure.
// This fact has surprising ramifications, as taking advantage of these
// repeats can greatly reduce the amount of space that the guess calculator
// framework needs both in memory and on disk.  However, it comes with a
// significant increase in cost in computational complexity.
//
// A "pattern" is an instance of a structure with specific terminal groups
// specified.  Each nonterminal can produce multiple terminal groups, so a
// pattern is a specific collection of terminal groups.
//
// "Pattern compaction" is the name I assigned to the concept of collecting
// all permutations of terminal groups into a single "pattern" object.  For
// example, take a pattern "ABC" where A, B, and C are terminal groups all
// derived from the same nonterminal U.  Even though A, B, and C have differing
// probabilities (by the definition of terminal groups), the patterns {ACB,
// BAC, BCA, CAB, CBA} all share the same probability as pattern "ABC".
// Therefore, we save space in pattern generation by only outputting "ABC",
// its probability, and the total strings produced under all 6 permutations.
// This is pattern compaction.  Note that this cannot be done with arbitrary
// patterns.  For example, in the pattern "A1!", where A, 1, and ! are all
// derived from different nonterminals, the pattern "1A!" is unlikely to share
// the same probability, because it was produced by a different structure.
//
// To aid in pattern compaction, this class will keep track of repeated
// nonterminals within a structure by assigning each nonterminal a group
// id.  We then maintain a sequence of group ids that aligns to the
// sequence of nonterminals in the structure.  For convenience, we also
// maintain a count of each group id, and functions for computing the
// number of permutations of a given structure, where permutations only
// operate within groups, i.e., a value cannot be permuted from a position
// with one group id to another position with a different group id.
//
// Pattern management is complex and requires coordinating between three
// data structures: an array of nonterminal pointers that comprises the
// structure of the pattern, an array of group ids and associated counts
// used to quickly identify repeating nonterminals, and a mixed-radix
// number that identifies which terminal groups are used in the current
// pattern.
//
// Pattern compaction is implemented as follows.  If a pattern is not in
// some canonical form, e.g. "BAC" instead of "ABC", we ignore it while
// iterating over the space of patterns.  If the pattern is in canonical
// form, we use it to represent P * the number of strings that it
// represents on its own, where P is the number of permutations of the
// pattern that share the same probability.  This number can be derived
// using standard permutation counting formulas, and is aided by a
// hardcoded factorial table.
//
// In addition, this class will handle "lookups" of input strings,
// specifically the assignment of a rank within the total number of
// strings produced by the structure associated with the pattern manager
// object.  This requires knowledge of pattern compaction, so is best
// served by this class.
//
#ifndef PATTERN_MANAGER_H__
#define PATTERN_MANAGER_H__

#include <gmp.h>
#include <string>
#include <unordered_map>
#include <map>
#include <cstdint>

#include "gcfmacros.h"
#include "lookup_data.h"
#include "nonterminal.h"
#include "mixed_radix_number.h"
#include "terminal_group.h"

class PatternManager {
public:
  // Initialization is complex, so it is deferred to an Init method
  PatternManager():
    nonterminals_(NULL),
    structure_size_(0),
    base_probability_(0.0),
    group_ids_(NULL),
    pattern_counter_(NULL),
    has_repeats_(false) {}
  ~PatternManager();

  // Use the given structure properties to initialize class variables
  bool Init(const std::string& representation,
            const char structurebreakchar,
            const unsigned int structure_size,
            Nonterminal* *nonterminals,
            const double base_probability);

  // Reset the pattern to the "first" or beginning pattern
  void resetPatternCounter();
  // Move to the next pattern - return false on overflow
  bool incrementPatternCounter();
  // Move to the next pattern whose probability might be higher than the
  // current pattern - return false on overflow
  bool intelligentSkipPatternCounter();

  // Get the first string that would be produced by the current pattern
  std::string getFirstStringOfPattern() const;
  // The lookup table will only contain the first string of a permutation, but
  // if we want to match a pattern to a lookup table entry, getFirstStringOfPattern
  // will not work if the current pattern is not the first permutation.
  std::string getCanonicalizedFirstStringOfPattern() const;

  // Get its probability
  double getPatternProbability() const;
  // Get the number of strings it would produce
  // By convention, mpz_t types are not returned, but are passed by reference
  // See http://stackoverflow.com/a/13396028
  void countStrings(mpz_t result) const;
  // Get an array of iterators for terminals of the current pattern
  TerminalGroup::TerminalGroupStringIterator** getStringIterators() const;

  // Check if the current pattern is the first of a permutation
  // This returns true if the pattern cannot be compacted
  bool isFirstPermutation() const;
  // Get the number of permutations of the current pattern
  // This returns 1 if the pattern cannot be compacted
  void countPermutations(mpz_t result) const;

  // Given a vector of terminals, look up the terminals and then perform
  // several calculations to determine the rank of this inputstring in
  // the full set of strings*permutations that can be produced by this
  // pattern.
  //
  // Note: this is not a const method and will change the value of the
  // pattern counter to one which can produce the given terminals. Hence
  // the longer name than for other lookup methods in the guess calculator
  // framework.
  LookupData* lookupAndSetPattern(const std::string *const terminals);


private:
  static const uint64_t kFactorialTable[21];  // This is assigned in the .cpp file
  static const unsigned int kMaxFactorial = 20;  // 21! is larger than UINT64_MAX

  // Helper Init function
  bool InitGroupIDsAndCounts(const std::string& representation,
                        const char structurebreakchar,
                        const unsigned int structure_size);

  // Used by the lookup method -- return the rank of this pattern in its
  // possible permutations, i.e., if isFirstPermutation is true, the rank will
  // be 0.  Otherwise, this will return a number from 1 to countPermutations.
  void getPermutationRank(mpz_t result) const;

  // Return a hash of hashes for each repeating group in the current pattern
  std::map<unsigned int, std::map<uint64_t, unsigned int>>*
      getCountsWithinRepeatingGroups() const;
  // Given a hash of digits to counts for a single nonterminal (such as one of
  // the hashes returned by getCountsWithinRepeatingGroups), return the number
  // of permutations.
  void getPermutationsOfGroup(mpz_t result,
                              std::map<uint64_t, unsigned int>* counts) const;

  // For all permutations of a pattern, the theoretical probabilities are the
  // same.  However, since we are dealing with small floating-point numbers
  // the order in which they are multiplied is actually important.
  // The following method returns the probability of the first permutation
  // of the current pattern.
  double getCanonicalizedPatternProbability() const;

  // Used by getCanonicalized* methods -- return a copy of the current
  // pattern_counter_ permuted so that isFirstPermutation will be true
  MixedRadixNumber* canonicalizePattern() const;

  // Given a pattern counter, check if the current pattern is the first
  // of a permutation.  Used by isFirstPermutation and as a check in
  // canonicalizePattern.
  bool checkFirstPermutation(MixedRadixNumber* pattern_counter) const;

  // Structure = a sequence of nonterminal pointers with a given probability
  Nonterminal* *nonterminals_;
  unsigned int structure_size_;
  double base_probability_;

  // A sequence of group ids that aligns with the structure
  unsigned int *group_ids_;
  // A map from current group ids to their counts in the structure
  std::unordered_map<unsigned int, unsigned int> group_counts_;

  // We will iterate through the structure using a mixed-radix number which
  // also aligns with the structure (i.e., is of size structure_size_)
  MixedRadixNumber* pattern_counter_;

  // Are any nonterminals repeated?  If not, many complex operations can be
  // avoided.
  bool has_repeats_;

  // Disable copy and assignment
  DISALLOW_COPY_AND_ASSIGN(PatternManager);
};


#endif // PATTERN_MANAGER_H__
