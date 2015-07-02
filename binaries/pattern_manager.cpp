// pattern_manager.cpp - a class for managing "groups" of nonterminals
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
// Modified: Thu May 29 14:52:57 2014
// 
// See header file for additional information

// Includes not covered in header file
#include <sstream>
#include <cstring>
#include <cstdlib>
#include <queue>

#include "pattern_manager.h"

// These constants are used for computing permutations
// Non-literal static class members cannot be initialized in the class body
const uint64_t PatternManager::kFactorialTable[21] = {
  1, 1, 2, 6, 24, 120, 720, 5040, 40320, 362880, 3628800, 39916800,
  479001600, 6227020800, 87178291200, 1307674368000, 20922789888000,
  355687428096000, 6402373705728000, 121645100408832000, 2432902008176640000
};


// Destructor
PatternManager::~PatternManager() {
  delete[] group_ids_;
  if (pattern_counter_ != NULL) {
    delete pattern_counter_;
  }

}


// Init routine
// Parse the structures string representation to initialize the object
// Return false on any failure
bool PatternManager::Init(
    const std::string& representation,
    const char structurebreakchar,
    const unsigned int structure_size,
    Nonterminal* *nonterminals,
    const double base_probability) {

  // Copy simple input variables
  nonterminals_ = nonterminals;
  base_probability_ = base_probability;

  // Initialize mixed-radix number
  // To facilitate iterating over all combinations of terminal groups produced
  // by this structure, we use a mixed-radix number.  It is initialized using
  // the number of terminal groups produced by each nonterminal.
  uint64_t *num_groups = new uint64_t[structure_size];
  for (unsigned int i = 0; i < structure_size; ++i)
    num_groups[i] = nonterminals[i]->countTerminalGroups();
  pattern_counter_ = new MixedRadixNumber(num_groups, structure_size);
  delete[] num_groups;

  // Initialize class variables related to group counts (counts of repeated
  // nonterminals in the structure)
  // This uses the string representation of the structure to identify repeats
  // Alternatively, we could look at the nonterminal pointers and check
  // for equivalence.  This is likely to work, but depends on implementation
  // details that I am not willing to depend on.
  if (!InitGroupIDsAndCounts(representation, 
                             structurebreakchar, 
                             structure_size)) {
    return false;
  }

  return true;
}


// Helper for Init routine
// This function initializes the group_ids_, group_counts_, has_repeats_,
// is_giant_, and structure_size_ class variables
bool PatternManager::InitGroupIDsAndCounts(
    const std::string& representation,
    const char structurebreakchar,
    const unsigned int structure_size) {

  // Set group_ids and group_counts
  group_ids_ = new unsigned int[structure_size];

  // Use a stringstream to parse the representation string using the
  // structurebreakchar as a delimiter
  std::istringstream structurestream(representation);
  std::string nonterminal_symbol;
  structure_size_ = 0;

  // Simultaneously, use a hash table to keep track of which nonterminals have
  // been seen.
  std::unordered_map<std::string, unsigned int> seen_nonterminal_ids;
  unsigned int next_group_id = 1;

  // Iterate over nonterminal symbols in structure to identify repeats
  while (getline(structurestream, nonterminal_symbol, structurebreakchar)) {
    // Seen before?
    if (seen_nonterminal_ids.count(nonterminal_symbol) > 0) {
      // This is a repeat
      has_repeats_ = true;
      unsigned int seengroup = seen_nonterminal_ids.at(nonterminal_symbol);
      group_ids_[structure_size_] = seengroup;
      ++group_counts_[seengroup];      
    } else {
      // This is a new nonterminal
      seen_nonterminal_ids[nonterminal_symbol] = next_group_id;
      group_ids_[structure_size_] = next_group_id;
      group_counts_[next_group_id] = 1;
      ++next_group_id;
    }
    ++structure_size_;
  }  // while (getline(...))
  if (structure_size_ != structure_size) {
    // This means that parsing failed somehow
    return false;
  }

  return true;
}


// Reset the pattern counter to the "start" which will correspond to the
// highest-probability pattern (because nonterminals should have their
// terminal groups in descending probability order)
void PatternManager::resetPatternCounter() {
  pattern_counter_->clear();
}


// Increment the pattern counter
bool PatternManager::incrementPatternCounter() {
  return pattern_counter_->increment();
}


// Move to the next pattern whose probability might be higher than the
// current pattern
bool PatternManager::intelligentSkipPatternCounter() {
  return pattern_counter_->intelligentSkip();
}


// Using the current state of the pattern counter, return the first string
// of the pattern.  This is done by simply returning the first strings of
// each of the terminal groups pointed to by the counter, and concatenating
// them together.
std::string PatternManager::getFirstStringOfPattern() const {
  std::string result("");
  for (unsigned int i = 0; i < structure_size_; ++i) {
    // Get current digit in this place from the pattern counter
    uint64_t group_index = pattern_counter_->getPlace(i);
    // Grab the corresponding string and append
    result.append(nonterminals_[i]->getFirstStringOfGroup(group_index));
    if (i < structure_size_ - 1) {
      result.append("\x01");
    }
  }
  return result;
}


// Using the current state of the pattern counter, return the first string
// of the pattern.  This is done by simply returning the first strings of
// each of the terminal groups pointed to by the counter, and concatenating
// them together.
std::string PatternManager::getCanonicalizedFirstStringOfPattern() const {
  std::string result("");
  MixedRadixNumber* canonical_counter = canonicalizePattern();

  for (unsigned int i = 0; i < structure_size_; ++i) {
    // Get current digit in this place from the pattern counter
    uint64_t group_index = canonical_counter->getPlace(i);
    // Grab the corresponding string and append
    result.append(nonterminals_[i]->getFirstStringOfGroup(group_index));
    if (i < structure_size_ - 1) {
      result.append("\x01");
    }
  }

  delete canonical_counter;
  return result;
}


// The probability of the current pattern is the product of current terminal
// group probabilities with the base (structure) probability
double PatternManager::getPatternProbability() const {
  double probability = base_probability_;
  for (unsigned int i = 0; i < structure_size_; ++i) {
    uint64_t group_index = pattern_counter_->getPlace(i);
    probability *= (nonterminals_[i]->getProbabilityOfGroup(group_index));
  }
  return probability;
}


// The following method returns the probability of the first permutation
// of the current pattern, by creating a canonicalized pattern counter
// and using this to compute the probability.
double PatternManager::getCanonicalizedPatternProbability() const{
  double probability = base_probability_;
  MixedRadixNumber* canonical_counter = canonicalizePattern();

  for (unsigned int i = 0; i < structure_size_; ++i) {
    uint64_t group_index = canonical_counter->getPlace(i);
    probability *= (nonterminals_[i]->getProbabilityOfGroup(group_index));
  }

  delete canonical_counter;
  return probability;
}


// Used by getCanonicalized* methods -- return a copy of the current
// pattern_counter_ permuted so that isFirstPermutation will be true
//
// The basic design of this method is as follows:
// 1. Create a hash of group ids to min-heaps.  This can be filled with
//    just one pass over the pattern.
// 2. Perform a second pass over the pattern, and for each element, extract-min
//    from the appropriate min-heap and place the value in the pattern.
// This should produce a pattern where, for each group, values are in ascending
// sorted order, which is the canonical order.  At the end, check that this
// function works as expected, otherwise die.
//
MixedRadixNumber* PatternManager::canonicalizePattern() const {
  MixedRadixNumber* canonical_counter = pattern_counter_->deepCopy();
  if (isFirstPermutation())
    return canonical_counter;

  // Define hash
  typedef std::priority_queue<uint64_t, 
                              std::vector<uint64_t>, 
                              std::greater<uint64_t>> MinHeap;
  std::unordered_map<unsigned int, MinHeap> sorted_group_values;

  // Make first pass to fill hash
  for (unsigned int i = 0; i < structure_size_; ++i) {
    unsigned int group_id = group_ids_[i];
    uint64_t group_index = canonical_counter->getPlace(i);
    // Add priority queue to the hash as needed
    if (sorted_group_values.count(group_id) == 0) {
      MinHeap child;
      sorted_group_values.insert(std::make_pair(group_id, child));
    }
    sorted_group_values[group_id].push(group_index);
  }

  // Make second pass to refill canonical counter
  for (unsigned int i = 0; i < structure_size_; ++i) {
    unsigned int group_id = group_ids_[i];
    if (sorted_group_values[group_id].empty()) {
      fprintf(stderr, "Empty priority queue found when canonicalizing pattern"
                      " in PatternManager::canonicalizePattern!\n");
      exit(EXIT_FAILURE);
    }
    // To pop off the top element of a priority queue, we have to call top()
    // and then pop()
    uint64_t sorted_group_index = sorted_group_values[group_id].top();
    sorted_group_values[group_id].pop();
    if (!canonical_counter->setPlace(i, sorted_group_index)) {
      fprintf(stderr, "Error setting place in canonical counter when "
                      "canonicalizing pattern in "
                      "PatternManager::canonicalizePattern!\n");
      exit(EXIT_FAILURE);
    }
  }

  if (!checkFirstPermutation(canonical_counter)) {
    fprintf(stderr, "After canonicalizing, checkFirstPermutation returns false"
                    " in PatternManager::canonicalizePattern!\n");
    exit(EXIT_FAILURE);    
  }

  return canonical_counter;
}


// The number of strings produced by the current pattern is the product of
// the number of strings produced by each current terminal group
void PatternManager::countStrings(mpz_t result) const {
  mpz_init(result);
  mpz_set_ui(result, 1);

  for (unsigned int i = 0; i < structure_size_; ++i) {
    uint64_t group_index = pattern_counter_->getPlace(i);
    mpz_t group_count;
    nonterminals_[i]->countStringsOfGroup(group_count, group_index);
    mpz_mul(result, result, group_count);
    mpz_clear(group_count);
  }
}


// Get an array of iterators for terminals of the current pattern
TerminalGroup::TerminalGroupStringIterator** PatternManager::getStringIterators()
   const {
  TerminalGroup::TerminalGroupStringIterator** iterators = 
    new TerminalGroup::TerminalGroupStringIterator*[structure_size_];

  for (unsigned int i = 0; i < structure_size_; ++i) {
    uint64_t group_index = pattern_counter_->getPlace(i);
    iterators[i] = nonterminals_[i]->getStringIteratorForGroup(group_index);
  }

  return iterators;  
}


// Check if the current pattern is the first of a permutation. I arbitrarily
// declare a permutation to be "first" if digits with each terminal group
// are monotonically increasing, e.g., "125" is first of all permutations:
// {125, 152, 215, 251, 512, 521}
//
bool PatternManager::isFirstPermutation() const {
  // If no repeats, than return true (no patterns have permutations)
  if (!has_repeats_)
    return true;

  return checkFirstPermutation(pattern_counter_);
}


// Private method
// Given a pattern counter for this structure, with the same group ID assignments
// as this object, check if the current pattern is the first of a permutation.
//
bool PatternManager::checkFirstPermutation(MixedRadixNumber* pattern_counter) const {
  // Use hash table to store values for each group ID and check for a decrease
  // in value
  std::unordered_map<unsigned int, uint64_t> group_digits;

  for (unsigned int i = 0; i < structure_size_; ++i) {

    unsigned int group_id = group_ids_[i];

    // Ignore groups that are not repeated -- they only have one element so are
    // automatically monotonic    
    if (group_counts_.at(group_id) > 1) {
      uint64_t digit = pattern_counter->getPlace(i);
      if (group_digits.count(group_id) > 0 &&
          digit < group_digits.at(group_id)) {
        // Current value is lower than the last saved value
        return false;
      }
      group_digits[group_id] = digit;
    }

  }

  return true;  
}


// NOTE: This is not a const method and will overwrite the current pattern
// counter.
// 
// Given a vector of terminals, look up the terminals and then perform
// several calculations to determine the rank of this inputstring in
// the full set of strings*permutations that can be produced by this
// pattern. Performing these calculations is easiest if we overwrite the
// current pattern counter.
//
// Returns a lookup_data structure that might have kUnexpectedFailure as
// a parse_status.  Callers to this method should check for this status
// and print an appropriate error message (or print the parse_status as
// an integer).
//
// Basic operation is as follows:
// 1. For each terminal, call the corresponding nonterminal's lookup method
//    gather the TerminalLookupData structs in an array.
// 2. Check that all lookups return kCanParse.  If not, return a LookupData
//    object with appropriate parse status.
// 3. Use terminal_group_index fields to set pattern counter.
// 4. Use terminal indexes as digits, and nonterminal->countStringsOfGroup
//    sizes as radices and convert to base-10 to get a rank of these terminals
//    in the current pattern (rank_in_pattern)
// 5. Get the number of strings in the current pattern using countStrings
//    (strings_in_pattern)
// 6. Get the rank of this pattern in the set of permutations of this pattern
//    using getPermutationRank (permutation_rank)
// 7. Get the rank of this string in all permutations of this pattern
//    LookupData->index = (permutation_rank*strings_in_pattern)+rank_in_pattern
//    Since all permutations of this pattern contain the same number of strings,
//    this will work.
// 8. Set other LookupData fields:
//    parse_status = kCanParse
//    source_ids = union of all terminal source_ids
//    probability = getCanonicalizedPatternProbability
//    first_string_of_pattern = getCanonicalizedFirstStringOfPattern
//
// Assume the size of terminals[] is the same as structure_size_
//
LookupData* PatternManager::lookupAndSetPattern(const std::string *const terminals) {
  LookupData* lookup_data = new LookupData;
  mpz_init(lookup_data->index);

  // Gather terminal lookups
  TerminalLookupData* *terminal_lookups = new TerminalLookupData*[structure_size_];
  for (unsigned int i = 0; i < structure_size_; ++i) {
    terminal_lookups[i] = nonterminals_[i]->lookup(terminals[i]);
  }

  // Check parse status
  for (unsigned int i = 0; i < structure_size_; ++i) {
    if (!(terminal_lookups[i]->parse_status & kCanParse)) {
      lookup_data->parse_status = terminal_lookups[i]->parse_status;
      lookup_data->probability = -1;
      mpz_set_si(lookup_data->index, -1);
      // Clear all terminal lookup mpzs and return
      for (unsigned int j = 0; j < structure_size_; ++j) {
        mpz_clear(terminal_lookups[j]->index);
        delete terminal_lookups[j];
      }
      delete[] terminal_lookups;
      return lookup_data;
    }
  }

  // Set pattern counter
  for (unsigned int i = 0; i < structure_size_; ++i) {
    if (!pattern_counter_->setPlace(i, terminal_lookups[i]->terminal_group_index)) {
      lookup_data->parse_status = kUnexpectedFailure;
      lookup_data->probability = -1;
      mpz_set_si(lookup_data->index, -1);
      // Clear all terminal lookup mpzs and return
      for (unsigned int j = 0; j < structure_size_; ++j) {
        mpz_clear(terminal_lookups[j]->index);
        delete terminal_lookups[j];
      }
      delete[] terminal_lookups;
      return lookup_data;      
    }
  }

  // Compute rank_in_pattern
  mpz_t rank_in_pattern;
  mpz_init_set_ui(rank_in_pattern, 0);
  for (unsigned int i = 0; i < structure_size_; ++i) {
    mpz_t strings_in_group;
    nonterminals_[i]->countStringsOfGroup(strings_in_group, 
                                          pattern_counter_->getPlace(i));
    // Use the formula from: http://stackoverflow.com/a/759319
    mpz_mul(rank_in_pattern, rank_in_pattern, strings_in_group);
    mpz_add(rank_in_pattern, rank_in_pattern, terminal_lookups[i]->index);

    mpz_clear(strings_in_group);
  }

  mpz_t strings_in_pattern;
  countStrings(strings_in_pattern);
  mpz_t permutation_rank;
  getPermutationRank(permutation_rank);

  // Compute the rank of this string
  mpz_mul(lookup_data->index, permutation_rank, strings_in_pattern);
  mpz_add(lookup_data->index, lookup_data->index, rank_in_pattern);
  // Clear intermediate values
  mpz_clear(strings_in_pattern);
  mpz_clear(rank_in_pattern);
  mpz_clear(permutation_rank);

  // Set other fields of lookup_data
  lookup_data->parse_status = kCanParse;
  lookup_data->probability = getCanonicalizedPatternProbability();
  lookup_data->first_string_of_pattern = getCanonicalizedFirstStringOfPattern();
  // Union source ids
  for (unsigned int i = 0; i < structure_size_; ++i) {
    lookup_data->source_ids.insert(terminal_lookups[i]->source_ids.begin(),
                                   terminal_lookups[i]->source_ids.end());
  }

  // Clear all terminal lookup mpzs and return
  for (unsigned int j = 0; j < structure_size_; ++j) {
    mpz_clear(terminal_lookups[j]->index);
    delete terminal_lookups[j];
  }
  delete[] terminal_lookups;
  return lookup_data;
}



// Use the formula for permutations of multisets to calculate the number of
// permutations for the current pattern
void PatternManager::countPermutations(mpz_t result) const {
  mpz_init_set_ui(result, 1);

  // Return 1 if this pattern has no repeats
  // (since this means it has no permutations)
  if (!has_repeats_)
    return;

  // For each group with repeats, we need to store counts for each digit of
  // that group
  std::map<unsigned int, std::map<uint64_t, unsigned int>> *counts_within_groups = 
    getCountsWithinRepeatingGroups();

  // We compute permutations within each repeated nonterminal group, and multiply
  // those counts together at each step to get a total count.
  for (auto it = counts_within_groups->begin(); 
            it != counts_within_groups->end();
            ++it) {    
    mpz_t group_perms;
    getPermutationsOfGroup(group_perms, &(it->second));
    mpz_mul(result, result, group_perms);
    mpz_clear(group_perms);
  }

  delete counts_within_groups;
}


// Return a hash of hashes for each repeating group in the current pattern,
// that counts the number of times specific terminal group ids repeat
// in this pattern.
std::map<unsigned int, std::map<uint64_t, unsigned int>>* 
    PatternManager::getCountsWithinRepeatingGroups() const {

  std::map<unsigned int,
    std::map<uint64_t, unsigned int>> *counts_within_groups =
    new std::map<unsigned int, std::map<uint64_t, unsigned int>>;

  for (unsigned int i = 0; i < structure_size_; ++i) {
    unsigned int group_id = group_ids_[i];
    if (group_counts_.at(group_id) > 1) {
      uint64_t digit = pattern_counter_->getPlace(i);      
      // Add new hash table for this group if needed
      if (counts_within_groups->count(group_id) == 0) {
        std::map<uint64_t, unsigned int> child;
        counts_within_groups->insert(std::make_pair(group_id, child));
      }

      // Check if this digit has been seen already
      if (counts_within_groups->at(group_id).count(digit) > 0) {
        (*counts_within_groups)[group_id][digit]++;
      } else {
        (*counts_within_groups)[group_id][digit] = 1;
      }
    }
  }

  return counts_within_groups;
}


// Given a hash of digits to counts for a single nonterminal (such as one of
// the hashes returned by getCountsWithinRepeatingGroups), return the number
// of permutations.
// 
// The number of permutations of a multiset = n! / m1!m2!m3!...mt!
// where n is the total size of the multiset, and m_i are the counts for
// each distinct value.  Using the terminology from Wikipedia, n is the total
// cardinality of the multiset, and m_i are the multiplicities of each member.
// 
void PatternManager::getPermutationsOfGroup(mpz_t result,
    std::map<uint64_t, unsigned int>* counts) const {
  mpz_init(result);

  // First, get n!
  unsigned long int total_count = 0;
  for (auto it = counts->begin(); it != counts->end(); ++it) {
    total_count += it->second;
  }
  if (total_count < kMaxFactorial)
    mpz_set_ui(result, kFactorialTable[total_count]);
  else
    mpz_fac_ui(result, total_count);

  // Now divide out the multiplicities
  for (auto it = counts->begin(); it != counts->end(); ++it) {
    if (it->second > 1) {  // Skip dividing by 1
      if (it->second < kMaxFactorial)
        mpz_div_ui(result, result, kFactorialTable[it->second]);
      else {
        mpz_t temp_factorial;
        mpz_init(temp_factorial);
        mpz_fac_ui(temp_factorial, it->second);

        mpz_div(result, result, temp_factorial);

        mpz_clear(temp_factorial);
      }
    }
  }
}


// Used by the lookup method -- return the rank of this pattern in its
// possible permutations, i.e., if isFirstPermutation is true, the rank will
// be 0.  Otherwise, this should return a number from 1 to countPermutations - 1.
//
// The general algorithm used here is based on the code given at:
// http://pavel.savara.sweb.cz/files/MultiSet.cs
// It uses a somewhat unexpected "magic" formula for computing rank.  I try to
// explain this below.  I do not know who originally discovered the formula :(
//
//
// Permutations of a multiset is given by the formula:
// n! / m1!m2!m3!...mt!  (as described in getPermutationsOfGroup)
//
// Say we are at position k in the string, where position 0 is the LSB.
// We use a formula that determines the "rank offset" contributed by the
// value at  position k, and then traverse down the string, adding up the
// offsets as we go to compute the total rank.
//
// To determine the amount of rank contributed by the value at position
// k, we want to sum the number of permutations of strings from (k-1) to
// 0 for those digit values lower than the current digit.  For example,
// if the string is "4112" as read in order of decreasing k, and k = 3,
// then the current value is "4".
//
// To get to a current value of "4" we run through permutations of the
// string where "1" is the current value and "2" is the current value.
// Let A = numPerms("124"), and B = numPerms("114").  For "1" as the
// current value, there are A permutations.  Next, "2" would be the
// current value, which yields B permutations.  So the rank offset we
// want is A + B.
//
// A and B are computed using the same multiset formula, but the string
// is smaller by one character, and the relevant multiplicity is also
// smaller by one character.
//
// Starting with,
// numPerms("4112") = 4! / 2!1!1!
//
// We remove 1 from n! and from the relevant m_i!:
// numPerms("124")  = 3! / 1!1!1!  = 6
// numPerms("114")  = 3! / 2!0!1!  = 3
//
// Assume that our multiset has its t distinct elements in sorted order.
// Let j correspond to the element of the current value.  In our example,
// the current value is "4" and the multiset has elements {"1", "2", "4"}
// (with multiplicities {2, 1, 1}), so j = 2.  We can then write a
// compact formula for rank offset as:
//
// \sum_{i = 0}^{j - 1} frac{n-1!}{m_1!...(m_i - 1)!...m_t!}
//
// We iterate over the values smaller than the current value, place it at
// the current value, and use the multiset permutation formula for the
// substring starting at (k-1) with new multiplicities.
//
// Now we can make a few observations to optimize the formula:
//
// 1. We can compute (n - 1)! from n! by dividing by n, which is the size
// of the current string.
//
// 2. We can compute (m_i - 1)! from m_i! by dividing by m_i which is the
// multiplicity of the current element.  Since m_i! is in the denominator
// of the fraction, we multiply the fraction by this multiplicity.
//
// Let current_perms be numPerms of the current string (numPerms("4112")
// in our example).  We can rewrite our sum as follows:
//
// \sum_{i = 0}^{j - 1} frac{n!}{m_1!...m_t!} * m_i / n
//
// = \sum_{i = 0}^{j - 1} current_perms * m_i / n
//
// = (current_perms / n) * \sum_{i = 0}^{j - 1} m_i
//
// The summation simply gives the weak ranking of the current value in
// the multiset (the sum of multiplicities of elements smaller than the
// current value).  This gives the simple formula:
//
// rank_offset = current_perms * weak_digit_rank / current_size
//
// The final optimization involves current_perms.  When we decrement k
// and move to the next position in the string it would be nice if we
// could update current_perms without recomputing the multiset
// permutation formula. We can do this with observations (1) and (2)
// above:
//
// new_perms = current_perms * m_j / current_size
//
// If these formulas are correct, all of these LHS values are integers.
// So if we multiply before dividing and use BigInts, we don't need to
// worry about the divisions creating fractions.
//
void PatternManager::getPermutationRank(mpz_t result) const {
  mpz_init_set_ui(result, 0);

  // Return 0 if this pattern has no repeats
  // (since this means it has no permutations)
  if (!has_repeats_)
    return;

  // To compute the permutation rank we need both counts within groups
  // and the permutation count of each group
  std::map<unsigned int, std::map<uint64_t, unsigned int>> *counts_within_groups =
    getCountsWithinRepeatingGroups();

  // Iterate over repeating groups
  // Since we are using a std::map, this will iterate in order of group_id
  for (auto it = counts_within_groups->begin(); 
       it != counts_within_groups->end(); ++it) {
    mpz_t rank;
    mpz_init_set_ui(rank, 0);

    // Get group id and permutations
    unsigned int group_id = it->first;
    mpz_t group_perms;
    getPermutationsOfGroup(group_perms, &(it->second));

    // Iterate over this groups' digits in the current pattern and compute the
    // permutation rank using the magic formula.  This destroys this groups'
    // hash in counts_within_groups.
    unsigned int k = 0;
    unsigned int current_size = group_counts_.at(group_id);
    mpz_t current_perms;
    // current_perms always stores the number of permutations of values from
    // k to the end of the string
    mpz_init_set(current_perms, group_perms);
    while (k < structure_size_ && mpz_cmp_ui(current_perms, 1) > 0) {
      // Skip ahead if in the wrong group
      if (group_ids_[k] != group_id) {
        ++k;
        continue;
      }

      // Get the current digit and its multiplicity (digit_count)
      uint64_t digit = pattern_counter_->getPlace(k);
      unsigned int digit_count = it->second.at(digit);

      // Determine the weak rank of this digit, i.e., the sum of the
      // multiplicities of those digits smaller than this one
      unsigned int weak_digit_rank = 0;
      // Since we have an ordered map, and the key is digits, we can iterate
      // over the map and compute a rolling sum until we get to this digit
      for (auto it2 = it->second.begin(); it2->first < digit; ++it2)
        weak_digit_rank += it2->second;

      // Now implement the magic formulas
      mpz_t tempval;
      mpz_init(tempval);
      // Formula for updating rank:
      // rank += (current_perms * weak_digit_rank / current_size)
      mpz_mul_ui(tempval, current_perms, weak_digit_rank);
      mpz_div_ui(tempval, tempval, current_size);
      mpz_add(rank, rank, tempval);
      mpz_clear(tempval);
      // current_perms *= digit_count; current_perms /= current_size
      mpz_mul_ui(current_perms, current_perms, digit_count);
      mpz_div_ui(current_perms, current_perms, current_size);
      --(it->second[digit]);  // Consume / reduce the multiplicity of this
                              // digit going forward
      
      --current_size;
      ++k;
    }
    mpz_clear(current_perms);

    // Sanity check, rank should be between 0 and group_perms - 1
    if (mpz_cmp(rank, group_perms) >= 0) {
      fprintf(stderr, "Unexpected error when computing permutation rank"
                      " in PatternManager::getPermutationRank!\n");
      exit(EXIT_FAILURE);
    }

    // We now have the rank of this permutation, and the total number of
    // possible permutations.  Treat the rank as a digit in a mixed-radix
    // number and compute overall rank in homogeneous base.
    mpz_mul(result, result, group_perms);
    mpz_add(result, result, rank);

    mpz_clear(group_perms);
    mpz_clear(rank);
  }

  delete counts_within_groups;
}
