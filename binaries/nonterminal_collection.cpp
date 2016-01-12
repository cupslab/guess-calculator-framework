// nonterminal_collection.cpp - a simple container class for Nonterminal objects
//   to ensure that the same object is not needlessly instantiated multiple
//   times.
//
// Use of this source code is governed by the GPLv2 license that can be found
// in the LICENSE file.
//
// Version 0.1
// Author: Saranga Komanduri
//   Based on code originally written and published by Matt Weir under the
//   GPLv2 license.
//
// Modified: Wed May 28 17:07:11 2014
//
// See header file for additional information

#include "nonterminal_collection.h"

// Declare static class member
std::unordered_map<std::string, Nonterminal *>
  NonterminalCollection::nonterminal_collection_;

// Destroy all Nonterminal objects in the collection
NonterminalCollection::~NonterminalCollection() {
  for (auto it = nonterminal_collection_.begin();
            it != nonterminal_collection_.end();
            ++it) {
    delete it->second;
  }
}


// Return the pointer to a Nonterminal object if it exists in the map (indexed
// by the given representation), otherwise create it.  If the element cannot be
// created, return NULL.
Nonterminal* NonterminalCollection::getOrCreateNonterminal(
    const std::string& representation) {
  if (nonterminal_collection_.count(representation) < 1) {
    // Create the element
    // fprintf(stderr,
    //   "Loading nonterminal represented by %s...",
    //   representation.c_str());
    Nonterminal *newnonterminal = new Nonterminal();
    if (!newnonterminal->loadNonterminal(representation, terminals_folder_)) {
      return NULL;
    } else {
      nonterminal_collection_.insert(
        std::make_pair(representation, newnonterminal)
      );
    }
    // fprintf(stderr, "done!\n");
  }

  return nonterminal_collection_.at(representation);
}
