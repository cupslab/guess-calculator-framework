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
// Modified: Wed May 28 17:07:11 2014
//

// This file implements the Structure class which is a sequence of nonterminals
// along with an associated probability.  It corresponds to the RHS of a
// start symbol production and the associated probability of that production
// rule.
//

#ifndef STRUCTURE_H__
#define STRUCTURE_H__

#include <gmp.h>
#include <string>
#include <cstdint>
#include <random>

#include "gcfmacros.h"
#include "lookup_data.h"
#include "nonterminal.h"
#include "nonterminal_collection.h"
#include "pattern_manager.h"
#include "pcfg.h"

// Forward declare class because we have circular includes to make generateStrings
// work (it needs to query the parent PCFG for each string if we want accurate
// probabilities)
class PCFG;

class Structure {
public:
  // As with the other grammar objects, this object is not initialized in the
  // constructor since initialization is a complex operation.
  Structure():
    nonterminals_(NULL),
    source_ids_(""),
    representation_(""),
    probability_(0.0),
    nonterminals_size_(0) {}
  ~Structure();

  // Initializer routine - loads the nonterminals from the grammar/terminalRules folder
  // Returns false on failure
  bool loadStructure(const std::string& representation,
                     const double probability,
                     const std::string& source_ids,
                     NonterminalCollection* nonterminal_collection);

  // By convention, mpz_t types are not returned, but are passed by reference
  // See http://stackoverflow.com/a/13396028
  void countStrings(mpz_t result) const;
  bool generatePatterns(const double cutoff) const;
  // generateStrings has two "modes": returning the probability under this structure
  // and returning an "accurate" probability in which the probability of each string
  // under all structures is accumulated.  The second mode requires "calling up" to
  // the PCFG to query all structures, so we use a parent object.  Otherwise, we
  // have to generate some array of strings to send back to the PCFG and this might
  // not fit in memory.
  bool generateStrings(const double cutoff,
                       const bool accurate_probabilities = false,
                       const PCFG* parent = NULL) const;

  // Mersenne twister engine is used for random numbers here because they
  // should create the same numbers on different systems, and because they are
  // fast. It would be good to use an abstract random number engine here if
  // possible.
  bool generateRandomStrings(const uint64_t number,
                             std::mt19937 generator,
                             const bool pattern_compaction = false,
                             const bool accurate_probabilities = false,
                             const PCFG* parent = NULL) const;

  std::string
    convertStringToStructureRepresentation(const std::string& inputstring) const;

  // Count the number of ways the input string could be parsed by this structure
  // Returns 0 if the string cannot be parsed
  uint64_t countParses(const std::string& inputstring) const;

  // Given a string, determine if it can be produced by this structure and
  // return a LookupData struct with relevant fields set
  LookupData* lookup(const std::string& inputstring) const;

  double getProbability();

private:
  uint64_t generateRandomStringNoPatternCompactionHelper
    (PatternManager* pattern_manager,
     std::vector<double>& random_numbers,
     const bool accurate_probabilities,
     const PCFG *const parent) const;

  uint64_t generateRandomStringPatternCompactionHelper
    (PatternManager* pattern_manager,
     std::vector<double>& random_numbers) const;

  // This must match the value used when the grammar was written
  static const char kStructureBreakChar = 'E';

  // Structures are implemented as a sequence of nonterminal pointers.
  Nonterminal* *nonterminals_;
  std::string source_ids_;
  std::string representation_;
  double probability_;
  unsigned int nonterminals_size_;

  // Disable copy and assignment
  DISALLOW_COPY_AND_ASSIGN(Structure);
};


#endif // STRUCTURE_H__
