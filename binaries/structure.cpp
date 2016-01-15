// structure.h - a class for "structures," a construct used in the specific
//   flavor of PCFG used in the guess calculator framework
//
// Use of this source code is governed by the GPLv2 license that can be found
//   in the LICENSE file.
//
// Version 0.1
// Author: Saranga Komanduri
//   Based on code originally written and published by Matt Weir under the
//   GPLv2 license.
//
// Modified: Sat May 31 15:51:48 2014
//
// See header file for additional information

// Includes not covered in header file
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <inttypes.h>
#include <sstream>
#include <stdint.h>

#include "pattern_manager.h"
#include "terminal_group.h"
#include "grammar_tools.h"

#include "structure.h"

// Destructor for Structure
// Nonterminal destruction happens in the NonterminalCollection destructor, so
// we can just free the nonterminal array here.
//
// Check nonterminals_size_ in case LoadStructure was not called yet.
Structure::~Structure() {
  if (nonterminals_size_ > 0)
    delete[] nonterminals_;
}


// Init routine will use the given structure and create and load the nonterminal
// objects that comprise it.
//
// Returns true on success
bool Structure::loadStructure(const std::string& representation,
                   const double probability,
                   const std::string& source_ids,
                   NonterminalCollection* nonterminal_collection) {
  // Assign values to relevant class variables
  representation_ = representation;
  probability_    = probability;
  source_ids_     = source_ids;

  // Parse the representation string and create nonterminal array
  unsigned int ntcounter = 1;
  for (unsigned int i = 0; i < representation_.length(); ++i) {
    if (representation_[i] == kStructureBreakChar) {
      ntcounter++;
    }
  }
  nonterminals_size_ = ntcounter;
  nonterminals_ = new Nonterminal*[nonterminals_size_];

  // Iterate through the representation and initialize each nonterminal
  ntcounter = 0;
  std::istringstream structurestream(representation_);
  std::string nonterminal_representation;
  while (getline(structurestream,
                 nonterminal_representation,
                 kStructureBreakChar)) {
    // The nonterminal_representation is the "key" to getting the correct
    //   Nonterminal object.
    Nonterminal *curnonterminal =
      nonterminal_collection->getOrCreateNonterminal(nonterminal_representation);

    // Die if the nonterminal could not be retrieved
    if (curnonterminal == NULL) {
      fprintf(stderr,
        "Error calling LoadNonterminal on nonterminal \"%s\"!\n",
        nonterminal_representation.c_str());
      return false;
    }

    // Passed! Store this nonterminal
    nonterminals_[ntcounter] = curnonterminal;
    ++ntcounter;
  }
  if (ntcounter != nonterminals_size_) {
    // There is no reason ntcounter should not match nonterminals_size_
    // but we check it anyway just to make sure.
    fprintf(stderr,
      "Found too few nonterminals in structure \"%s\"!\n",
      representation_.c_str());
    return false;
  }

  return true;
}


// Return the total count of strings for this structure
//
// Since this is a context free grammar, we can simply multiply the string
// counts over the nonterminals
void Structure::countStrings(mpz_t result) const {
  mpz_init_set_ui(result, 1);

  for (unsigned int i = 0; i < nonterminals_size_; ++i) {
    mpz_t nonterminal_count;
    nonterminals_[i]->countStrings(nonterminal_count);
    mpz_mul(result, result, nonterminal_count);
    mpz_clear(nonterminal_count);
  }
}


// Generate all "patterns" from this structure whose probability is above
// the given cutoff.
// Output to stdout.
//
// A "pattern" is a set of sequences of TerminalGroups that share a
// probability, so this function could simply iterate over all terminal
// groups and output those patterns with probability above the cutoff.
// However, this operation is made more complex by an optimization I
// introduced called "pattern compaction." If the same nonterminal is
// repeated multiple times in a pattern, then their corresponding
// terminal groups could be swapped and the result would have the same
// probability.  Accounting for all possible permutations of a pattern
// greatly reduces the output of this function with no loss in accuracy.
// It does greatly increase the complexity of the code, though.
//
// Return true on success.
//
bool Structure::generatePatterns(const double cutoff) const {
  // To facilitate iterating over all combinations of terminal groups produced
  // by this structure, we use a very specialized structure called a
  // PatternManager.  This structure will handle the complexity of
  // generating patterns, including pattern compaction and intelligent skipping
  PatternManager *pattern_manager = new PatternManager;
  if (!pattern_manager->Init(representation_,
                            kStructureBreakChar,
                            nonterminals_size_,
                            nonterminals_,
                            probability_)) {
    return false;
  }

  // Now that we have the pattern manager, use it to iterate over patterns and
  // output them to stdout
  bool patterns_left = true;
  while (patterns_left) {

    // First check if the current pattern is below the cutoff, if not use
    // intelligent skipping to jump ahead
    double pattern_probability = pattern_manager->getPatternProbability();
    if (pattern_probability < cutoff) {
      patterns_left = pattern_manager->intelligentSkipPatternCounter();
      continue;
    }

    // Above cutoff, so output pattern unless it is not a first permutation
    if (pattern_manager->isFirstPermutation()) {
      // Compute number of strings this pattern and all permutations would
      // produce (this is pattern compaction)
      mpz_t string_count;
      pattern_manager->countStrings(string_count);
      mpz_t permutation_count;
      pattern_manager->countPermutations(permutation_count);
      mpz_t total_count;
      mpz_init(total_count);
      mpz_mul(total_count, string_count, permutation_count);
      char counterstring[1024];
      // Write out the GMP number to a C-style string in base 10
      mpz_get_str(counterstring, 10, total_count);

      // Get the pattern identifier -- I use the first string that would be
      // produced by the pattern
      std::string pattern_representation =
        pattern_manager->getFirstStringOfPattern();

      // Output to stdout
      printf("%a\t%s\t%s\n", pattern_probability, counterstring,
                             pattern_representation.c_str());
      mpz_clear(string_count);
      mpz_clear(permutation_count);
      mpz_clear(total_count);
    }
    patterns_left = pattern_manager->incrementPatternCounter();
  }

  // If we are here, we have iterated over the complete space of patterns
  // covered by this structure!
  delete pattern_manager;
  return true;
}


double Structure::getProbability() {
  return probability_;
}


// Generate all strings from this structure whose probability is above
// the given cutoff.
// Output to stdout.
//
// This uses the same structure as generatePatterns, because using patterns
// allows us to use the intelligentSkip function to traverse the space more
// quickly.  Pattern compaction is ignored.
//
// Return true on success.
//
bool Structure::generateStrings(
    const double cutoff,
    const bool accurate_probabilities,
    const PCFG *const parent) const {
  // Initialize pattern manager
  PatternManager *pattern_manager = new PatternManager;
  if (!pattern_manager->Init(representation_,
                            kStructureBreakChar,
                            nonterminals_size_,
                            nonterminals_,
                            probability_)) {
    return false;
  }

  // Iterate over all patterns
  bool patterns_left = true;
  while (patterns_left) {
    // Jump ahead if current pattern is below the cutoff
    double pattern_probability = pattern_manager->getPatternProbability();
    if (pattern_probability < cutoff) {
      patterns_left = pattern_manager->intelligentSkipPatternCounter();
      continue;
    }

    std::string first_string_of_pattern =
      pattern_manager->getCanonicalizedFirstStringOfPattern();

    // Iterate over all the strings that this pattern would produce using
    // TerminalGroupStringIterator objects
    TerminalGroup::TerminalGroupStringIterator **iterators =
      pattern_manager->getStringIterators();

    // Iterate until the first iterator overflows -- an overflow is indicated
    // by false returned from an increment call
    bool strings_left = true;
    while (strings_left) {
      // TODO: We can be more efficient here because typically only the last
      // piece of the current_string will change on each iteration

      // Build the current string and output to stdout or check and return
      std::string current_string = "";
      for (unsigned int i = 0; i < nonterminals_size_; ++i)
        current_string.append(iterators[i]->getCurrentString());
      if (accurate_probabilities) {
        LookupData *total_lookup = parent->lookupSum(current_string);

        // Check for catastrophic failure
        if ( (total_lookup->parse_status & kUnexpectedFailure) ||
             !(total_lookup->parse_status & kCanParse) ) {
          fprintf(stderr,
            "String generation returned a string that could not be parsed?!?"
            " for structure %s and inputstring %s!\n",
            representation_.c_str(), current_string.c_str());
          exit(EXIT_FAILURE);
        }

        // We only want one copy of each string to be produced, so only print
        // if the string in the lookup structure matches the current pattern
        // (this indicates that this structure was the highest probability
        // structure for this string).  We are sure that one of the string's
        // structures will print this string since it must be above the cutoff
        // for this structure (otherwise we wouldn't be producing it), so the
        // highest probability structure for this string must also have it
        // above the cutoff.
        if (total_lookup->first_string_of_pattern == first_string_of_pattern) {
          printf("%a\t%s\n", total_lookup->probability,
                             current_string.c_str());
        }
      } else {
        printf("%a\t%s\n", pattern_probability,
                           current_string.c_str());
      }

      // Increment the string iterators as needed
      long int iter_index = nonterminals_size_ - 1;
      while (!iterators[iter_index]->increment()) {
        // This iterator overflowed, reset it and increment the previous one
        iterators[iter_index]->restart();
        --iter_index;
        if (iter_index < 0) {
          // The most significant iterator overflowed, we have to stop here
          strings_left = false;
          break;
        }
      }
    }
    // Free iterators
    for (unsigned int i = 0; i < nonterminals_size_; ++i)
      delete iterators[i];
    delete[] iterators;

    patterns_left = pattern_manager->incrementPatternCounter();
  }

  // If we are here, we have iterated over the complete space of patterns
  // covered by this structure!
  delete pattern_manager;
  return true;
}


// Generate strings randomly according to the probability distribution of this
// PCFG. This method is designed to be used with Monte Carlo methods for guess
// number calculation.
bool Structure::generateRandomStrings(const uint64_t number,
                                      std::mt19937 generator) const {
  for (unsigned int i = 0; i < number; i++) {
    std::string password = "";
    double probability = probability_;
    for (unsigned int j = 0; j < nonterminals_size_; j++) {
      Nonterminal* nonterminal = nonterminals_[j];
      uint64_t terminal_group =
        nonterminal->produceRandomTerminalGroup(generator);
      probability *= nonterminal->getProbabilityOfGroup(terminal_group);
      password.append
        (nonterminal->produceRandomStringOfGroup(terminal_group, generator));
    }
    printf("%a\t%s\n", probability, password.c_str());
  }
  return true;
}


// Convert a string to a "representation" of non-terminal symbols in the grammar
// Note that this function is extremely specific to the particular restricted
// PCFG used in the current guess calculator framework.
//
// If an \x01 character is found in the input, it is replaced with the break
// character.  If this behavior is not desired, use the
// StripBreakCharacterFromTerminal function from grammar_tools.h.
//
// If, for some reason, an error occurs during conversion, this function will
// call sys.exit with failure.
//
std::string Structure::convertStringToStructureRepresentation(
    const std::string& inputstring) const {
  std::string representation;
  representation.reserve(inputstring.size());
  for (unsigned int i = 0; i < inputstring.size(); ++i) {
    // Convert the inputstring to a USLDE structure
    if( inputstring[i] >= 'a' && inputstring[i] <= 'z' )
      representation.push_back('L');
    else if( inputstring[i] >= 'A' && inputstring[i] <= 'Z' )
      representation.push_back('U');
    else if( inputstring[i] >= '0' && inputstring[i] <= '9' )
      representation.push_back('D');
    else if( inputstring[i] == 1 )
      representation.push_back(kStructureBreakChar);
    else
      representation.push_back('S');
  }

  // representation should not exceed the size of inputstring
  if (representation.size() != inputstring.size()) {
    fprintf(stderr,
      "Error converting string to a structure representation: %s!\n",
      inputstring.c_str());
    exit(EXIT_FAILURE);
  }

  return representation;
}


// Given a string, determine if it can be produced by this structure and
// return a LookupData struct with relevant fields set
//
// With the current restricted PCFG setup, we can easily shortcut parsing.
// We can "determine" the nonterminals in a given string based solely on
// character-class composition, without the need to build a complete CYK
// parser.  However, if restrictions on the PCFG (specifically nonterminals)
// are loosened in the future, it will be necessary to rewrite this method.
//
// First, check that the structure representation of the input string matches
// this structure.  If not, set parse_status to kStructureNotFound and return.
// This comparison is performed using nonterminals only, because we want to
// ignore breaks in the structure on lookup.  This complicates the algorithm
// slightly because we need to track where we are in the input string and in the
// nonterminals, and make sure we have consumed all of both and never go out of
// bounds on the input string.
//
// Then use a PatternManager object to perform the rest of the lookups, because
// determining an index for the given string requires knowledge of pattern
// compaction, which is encapsulated in the PatternManager class.  However,
// the PatternManager doesn't know the enclosing structure source ids,
// so this is folded in to the LookupData before returning.
//
// If the \x01 character is found in the string it is ignored as it represents
// the structure break character in terminal strings.
//
// Die on any failures.
//
LookupData* Structure::lookup(const std::string& inputstring) const {
  // Remove any break characters from the input before parsing
  std::string unbroken_input =
    grammartools::StripBreakCharacterFromTerminal(inputstring);

  // Match the structure representation of the inputstring with the
  // representation of the nonterminals in this structure
  std::string inputstring_representation =
    convertStringToStructureRepresentation(unbroken_input);

  // Break the structure into terminals and check each terminal
  unsigned int string_position = 0;
  bool parseable = true;
  std::string *terminals = new std::string[nonterminals_size_];
  // Iterate over the nonterminals
  for (unsigned int i = 0; i < nonterminals_size_ && parseable; ++i) {
    // Check the nonterminal representation first
    std::string nonterminal_representation =
      nonterminals_[i]->getRepresentation();
    for (unsigned int j = 0; j < nonterminal_representation.size(); ++j) {
      // Make sure we are not going past the end of the inputstring
      if (string_position >= inputstring_representation.size()) {
        parseable = false;
        break;
      }
      if (inputstring_representation[string_position++] !=
          nonterminal_representation[j]) {
        parseable = false;
        break;
      }
    }
    // Extract this part of the input string
    if (parseable) {
      unsigned int length = nonterminal_representation.size();
      unsigned int start_position = string_position - length;
      terminals[i] = unbroken_input.substr(start_position, length).c_str();
    }
  }
  // Finally, check that there isn't more the inputstring that wasn't yet
  // captured
  if (parseable) {
    if (string_position != inputstring_representation.size())
      parseable = false;
  }
  if (!parseable) {
    delete[] terminals;
    // Make a new lookup_data object to return
    LookupData *lookup_data = new LookupData;
    mpz_init(lookup_data->index);
    lookup_data->parse_status = kStructureNotFound;
    lookup_data->probability = -1;
    mpz_set_si(lookup_data->index, -1);
    return lookup_data;
  }

  // Instantiate a pattern manager
  PatternManager pattern_manager;
  if (!pattern_manager.Init(representation_,
                            kStructureBreakChar,
                            nonterminals_size_,
                            nonterminals_,
                            probability_)) {
    fprintf(stderr,
      "Error instantiating pattern manager for structure %s and "
      "inputstring %s!\n",
      representation_.c_str(), inputstring.c_str());
    exit(EXIT_FAILURE);
  }
  LookupData *pattern_lookup = pattern_manager.lookupAndSetPattern(terminals);
  delete[] terminals;

  // Check for catastrophic failure
  if (pattern_lookup->parse_status & kUnexpectedFailure) {
    fprintf(stderr,
      "Pattern manager reported unexpected failure for structure %s and "
      "inputstring %s!\n",
      representation_.c_str(), inputstring.c_str());
    exit(EXIT_FAILURE);
  }
  // If inputstring was not parsed, return the struct
  if (!(pattern_lookup->parse_status & kCanParse)) {
    return pattern_lookup;
  }
  if (!grammartools::AddSourceIDsFromString(source_ids_,
                                            pattern_lookup->source_ids)) {
    fprintf(stderr,
      "Unable to add source ids \"%s\" for structure %s and "
      "inputstring %s to lookup data!\n",
      source_ids_.c_str(), representation_.c_str(), inputstring.c_str());
    exit(EXIT_FAILURE);
  }

  return pattern_lookup;
}


// Count the number of ways the given string could be parsed by this structure
//
// Returns 0 if the string cannot be parsed, otherwise returns 1.
//
uint64_t Structure::countParses(const std::string& inputstring) const {
  LookupData *pattern_lookup = lookup(inputstring);
  // Free the index since we don't need it
  mpz_clear(pattern_lookup->index);

  if (pattern_lookup->parse_status & kCanParse)
    return 1;
  else
    return 0;
}
