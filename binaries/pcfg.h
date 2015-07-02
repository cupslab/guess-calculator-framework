// pcfg.h - a class for working with the specific flavor of PCFG used in the
//   guess calculator framework
//
// Use of this source code is governed by the GPLv2 license that can be found
//   in the LICENSE file.
//
// Version 0.1
// Author: Saranga Komanduri
//   Based on code originally written and published by Matt Weir under the
//   GPLv2 license.
//
// Modified: Wed Sat Aug 30 23:14:26 2014
//

// This file implements the PCFG class, which is intended to encapsulate
// the implementation details of the PCFG used in the guess calculator
// framework.  However, it should not be construed as implementing the
// functionality needed to represent any arbitrary PCFG.  Rather, it takes
// advantage of a number of simplifying constraints on allowed PCFGs to
// increase parsing and string generation performance.  This follows in the
// design philosophy of Weir et al. who first demonstrated the use of
// restricted PCFGs in modeling and evaluating passwords.
//
// The class also defines a few limits on the grammar that is read from disk:
//   kMaxStructureLength = 40
//     Ignore any structures over length 40.  This will also ignore loading
//     of any associated terminals and nonterminals.  This greatly reduces
//     memory consumption.
//
// Note: Loading the grammar is too complex to go in the constructor, so it is
// deferred to an Init() routine. This loads the grammar from disk and also
// memory-maps large portions of it. The grammar must reside in grammar/
// and have a very specific structure, except for the structures file which
// is allowed to reside at any given path.
//

#ifndef PCFG_H__
#define PCFG_H__

#include <gmp.h>
#include <string>
#include <cstdint>

#include "gcfmacros.h"
#include "structure.h"
#include "nonterminal_collection.h"
#include "lookup_data.h"

// Forward declare class because we have circular includes
class Structure;

class PCFG {
 public:
  // Initialization is complex, so it is deferred to an Init method (LoadGrammar)
  //
  // Note that all class operations should still work even in this state, because
  // they will iterate using structures_size_ and then return zero or empty values.
  PCFG():
    structures_(NULL),
    structures_size_(0),
    nonterminal_collection_(NULL) {}
  ~PCFG();

  // Load the grammar from disk and initialize structures objects
  // This should be called before doing anything else with the PCFG object.
  //
  // Returns true on success, and dies on failure
  bool loadGrammar(
    const std::string& structuresfilename,
    // The following folder name must end in "/"
    const std::string& terminals_folder
    );

  // By convention, mpz_t types are not returned, but are passed by reference
  // See http://stackoverflow.com/a/13396028
  void countStrings(mpz_t result) const;
  bool generatePatterns(const double cutoff) const;
  bool generateStrings(const double cutoff, 
                       const bool accurate_probabilities = false) const;

  // Run lookups for each structure in the grammar and return a LookupData
  // struct with the "best" lookup (highest probability / summed probabilities)
  LookupData* lookup(const std::string& inputstring) const;
  LookupData* lookupSum(const std::string& inputstring) const;
  uint64_t countParses(const std::string& inputstring) const;



 private:
  // Implementation limits
  static const unsigned int kMaxStructureLength = 40;

  // Structures are the top-level productions of the grammar.  They are
  // nonterminal strings produced directly from the start symbol. In this
  // class we store pointers to structures, which themselves point at
  // downstream productions.
  //
  // I could use a vector here, but the array is never changed after
  // initialization, so this is more space-efficient.
  Structure *structures_;
  unsigned int structures_size_;

  // Structures are collections of nonterminals, but without a static
  // collection of them it is likely that the same data will be instantiated
  // multiple times.  The NonterminalCollection object will be passed down
  // to all structures so they can pull objects from a common collection.
  NonterminalCollection* nonterminal_collection_;


  // Disable copy and assignment
  DISALLOW_COPY_AND_ASSIGN(PCFG);
};


#endif  // PCFG_H__
