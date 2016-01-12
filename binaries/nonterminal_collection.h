// nonterminal_collection.h - a simple container class for Nonterminal objects
//   to ensure that the same object is not needlessly instantiated multiple
//   times.
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

// A PCFG contains many structures, and each structure contains many
// nonterminals.  Many nonterminals are shared between structures, however,
// so it is a waste of memory (and open file handles) to instantiate the same
// nonterminal multiple times for each different strucure that it appears in.
// By the rules of a context-free grammar, a given nonterminal can be used with
// the same production rules regardless of its context, which would be given by
// the structure it belongs to.
//


#ifndef NONTERMINAL_COLLECTION_H__
#define NONTERMINAL_COLLECTION_H__

#include <string>
#include <unordered_map>

#include "gcfmacros.h"
#include "nonterminal.h"

class NonterminalCollection {
 public:
  // This is the only constructor
  NonterminalCollection(const std::string& terminals_folder):
    terminals_folder_(terminals_folder) {}
  ~NonterminalCollection();

  Nonterminal* getOrCreateNonterminal(const std::string& representation);

 private:
  static std::unordered_map<std::string, Nonterminal *> nonterminal_collection_;
  const std::string terminals_folder_;

  // Disable copy and assignment
  DISALLOW_COPY_AND_ASSIGN(NonterminalCollection);
};


#endif  // NONTERMINAL_COLLECTION_H__
