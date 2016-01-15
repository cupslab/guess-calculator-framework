// pcfg.cpp - a class for working with the specific flavor of PCFG used in the
//   guess calculator framework
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

// Includes not covered in header file
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <errno.h>
#include <inttypes.h>
#include <random>

#include "grammar_tools.h"

#include "pcfg.h"

int LOGGING_FREQUENCY = 100;


// Destructor for PCFG
// Delete the structures assuming that structures_size_ is accurate
PCFG::~PCFG() {
  if (structures_size_ > 0)
    delete[] structures_;
  if (nonterminal_collection_ != NULL)
    delete nonterminal_collection_;
}


// Init routine will load the grammar from disk and initialize structures objects.
//
// We take a depth-first approach when initalizing objects in the grammar, i.e.,
// initialization of a structure requires initialization of nonterminal objects,
// which require terminal groups, etc.
//
// Returns true on success, but currently dies on failure.
bool PCFG::loadGrammar(
    const std::string& structuresfilename,
    const std::string& terminals_folder) {
  FILE *structurefile = fopen(structuresfilename.c_str(), "r");
  if (structurefile == NULL) {
    int saved_errno = errno;
    perror("Error opening structure file: ");
    fprintf(stderr,
      "Structures filename: %s\n", structuresfilename.c_str());
    // Check if there error is too many open files in the process and output a
    // special error message
    if (saved_errno == EMFILE) {
      fprintf(stderr,
        "Error: Increase the open file limit of the OS. See INSTALL.md\n");
    }
    exit(EXIT_FAILURE);
  }

  // Figure out the first blank line number and skip header line
  int blanklinepos = grammartools::CountLinesToNextBlankLine(structurefile);
  if (blanklinepos == -1) {
    fprintf(stderr,
      "Error parsing structure file: %s! No blank line found at end of structures block!",
      structuresfilename.c_str());
    exit(EXIT_FAILURE);
  }
  int headerlines = grammartools::SkipStructuresHeader(structurefile);
  if (headerlines < 0) {
    fprintf(stderr,
      "Error parsing structure file: %s! First line was not as expected!",
      structuresfilename.c_str());
    exit(EXIT_FAILURE);
  }

  // Allocate structures and Nonterminal collection
  if ((blanklinepos - headerlines - 1) < 0) {
    fprintf(stderr,
      "Error parsing structure file: %s! Not enough structure lines found!"
      "blanklinepos: %d  headerlines: %d\n",
      structuresfilename.c_str(), blanklinepos, headerlines);
    exit(EXIT_FAILURE);
  }
  structures_size_ = static_cast<unsigned>(blanklinepos - headerlines - 1);
  structures_ = new Structure[structures_size_];
  nonterminal_collection_ = new NonterminalCollection(terminals_folder);

  // Read remaining lines - structures are stored line-by-line in the file
  unsigned int structure_counter = 0;
  for (unsigned int i = 0; i < structures_size_; ++i) {
    std::string read_structure, read_source_ids;
    double read_probability;
    if (grammartools::ReadStructureLine(structurefile, read_structure,
                          read_probability, read_source_ids)) {
      // Don't load giant structures -- they have extremely low probability
      // so you waste a lot of RAM (because they are large) for little benefit
      if (read_structure.size() > kMaxStructureLength)
        continue;
      // LoadStructure will start a chain reaction that will initialize
      // nonterminals and terminals associated with this structure
      if (!structures_[structure_counter].loadStructure(
            read_structure,
            read_probability,
            read_source_ids,
            nonterminal_collection_)) {
        fprintf(stderr,
          "Error calling LoadStructure on structure \"%s\" in file \"%s\"!\n",
          read_structure.c_str(),
          structuresfilename.c_str());
        exit(EXIT_FAILURE);
      }
      ++structure_counter;
    } else {
      fprintf(stderr,
        "Error parsing structure file: %s! Structure line was not as expected (check for previous errors)!",
        structuresfilename.c_str());
      exit(EXIT_FAILURE);
    }
  }
  structures_size_ = structure_counter;

  fclose(structurefile);

  return true;
}


// Return the total count of strings over all structures
// Returns the mpz_t type which is a big integer type from the GMP library
void PCFG::countStrings(mpz_t result) const {
  mpz_init(result);

  for (unsigned int i = 0; i < structures_size_; ++i) {
    mpz_t structure_count;
    structures_[i].countStrings(structure_count);
    mpz_add(result, result, structure_count);
    mpz_clear(structure_count);
  }
}


// Print all patterns above the given probability cutoff to stdout
// Simply calls the corresponding routine of each structure object
//
// Return true on success
bool PCFG::generatePatterns(const double cutoff) const {
  for (unsigned int i = 0; i < structures_size_; ++i) {
    if (!structures_[i].generatePatterns(cutoff))
      return false;
  }
  return true;
}


// Print all strings above the given probability cutoff to stdout
//
// If accurate_probabilities is true, then Structure::generateStrings will
// look up every string that is generated and output an accurate string
// probability.  We need to send that method an appropriate object that it
// can make calls on.
//
// Return true on success
bool PCFG::generateStrings(const double cutoff,
                           const bool accurate_probabilities) const {
  for (unsigned int i = 0; i < structures_size_; ++i) {
    if (accurate_probabilities) {
      if (!structures_[i].generateStrings(cutoff,
                                          true,
                                          this))
        return false;
    } else {
      if (!structures_[i].generateStrings(cutoff))
        return false;
    }
  }
  return true;
}


bool PCFG::generateRandomStrings(const uint64_t number) const {

  // Create a random number generator and distribution
  // Replace rd() with a static seed if desired
  std::random_device rd;
  std::mt19937 mt_random_generator(rd());
  std::uniform_real_distribution<double> distribution(0.0, 1.0);
  std::vector<double> random_numbers(number);
  for (uint64_t i = 0; i < number; i++) {
    random_numbers[i] = distribution(mt_random_generator);
  }
  std::sort(random_numbers.begin(), random_numbers.end());

  uint64_t random_number_index = 0;
  double cumulative_probability = 0;
  uint64_t structure_logging_freq = structures_size_ / LOGGING_FREQUENCY;
  for (unsigned int i = 0;
       i < structures_size_ && random_number_index < number; ++i) {
    uint64_t assigned = 0;
    cumulative_probability += structures_[i].getProbability();
    while (random_numbers[random_number_index] <= cumulative_probability) {
      random_number_index += 1;
      assigned += 1;
      if (random_number_index >= number) {
        break;
      }
    }
    if (i % structure_logging_freq == 0) {
      fprintf(stderr, "Info: Currently handling structure %u of %u; Generated "
              "%" PRIu64 " passwords of %" PRIu64 "\n",
              i, structures_size_, random_number_index, number);
      // We want the logging to show up immediately and it shouldn't harm
      // performance too much
      fflush(stderr);
    }
    if (!structures_[i].generateRandomStrings(assigned, mt_random_generator))
      return false;
  }
  if (random_number_index < number) {
    fprintf(stderr,
            "Error: Random string generation did not produce as many strings as"
            " expected. This is a bug!?!?! Was expecting to generate %" PRIu64
            " passwords but generated %" PRIu64 " in "
            "PCFG::generateRandomStrings\n", number, random_number_index);
  }
  return true;
}

// Given a string, count up the ways it can be parsed over all structures
uint64_t PCFG::countParses(const std::string& inputstring) const {
  uint64_t numparses = 0;

  for (unsigned int i = 0; i < structures_size_; ++i) {
    numparses += structures_[i].countParses(inputstring);
  }

  return numparses;
}


// Lookup the given inputstring for each structure, and then "reduce" the
// returned LookupData structs to the one with lowest probability.
//
// Since there might be hundreds of thousands of structures, keep the best
// one at any given time.  Best is given by:
// 1. parseable (if current best is not parseable)
// 2. highest probability if parseable
// 3. highest parse_status code if not parseable
//
LookupData* PCFG::lookup(const std::string& inputstring) const {
  LookupData *lookup_data = new LookupData;

  // Pick a "low" initial value
  lookup_data->parse_status = kStructureNotFound;
  mpz_init(lookup_data->index);
  bool canParse = false;  // Is the current best parseable?

  for (unsigned int i = 0; i < structures_size_; ++i) {
    LookupData *structure_lookup = structures_[i].lookup(inputstring);

    // Implement three conditions that can make this structure better than the
    // current best structure.
    if ( (!canParse && (structure_lookup->parse_status & kCanParse))  ||
         (canParse &&
          (structure_lookup->parse_status & kCanParse) &&
          (lookup_data->probability < structure_lookup->probability)) ||
         (!canParse &&
          (static_cast<int>(lookup_data->parse_status) <
           static_cast<int>(structure_lookup->parse_status))) ) {
      // Cleanup current best and make this structure the new best
      mpz_clear(lookup_data->index);
      delete lookup_data;
      lookup_data = structure_lookup;
      canParse = (lookup_data->parse_status & kCanParse);
    } else {
      // Clear up this structure's lookup data and don't update current data
      mpz_clear(structure_lookup->index);
      delete structure_lookup;
    }
  }

  return lookup_data;
}



// Lookup the given inputstring for each structure, and then "reduce" the
// returned LookupData structs to one where all string probabilities from
// matching structures are added together.
//
// The general structure of this function is very similar to lookup().
//
LookupData* PCFG::lookupSum(const std::string& inputstring) const {
  LookupData *lookup_data = new LookupData;

  // Pick a "low" initial value
  lookup_data->parse_status = kStructureNotFound;
  mpz_init(lookup_data->index);
  bool canParse = false;  // Is the current best parseable?

  // Store the accumulated probability -- we want the returned structure
  // to have the highest probability among all structures, but return
  // the accumulated probability.  In this way, every string is tied to
  // its highest probability structure (so we can output the string only
  // once) but we return an accurate probability.
  double total_probability = 0;

  for (unsigned int i = 0; i < structures_size_; ++i) {
    LookupData *structure_lookup = structures_[i].lookup(inputstring);

    // If the structure could parse this string, add the probability
    // of the string under this structure.
    if (structure_lookup->parse_status & kCanParse) {
      total_probability += structure_lookup->probability;
    }

    // Implement three conditions that can make this structure better than the
    // current best structure.
    if ( (!canParse && (structure_lookup->parse_status & kCanParse))  ||
         (canParse &&
          (structure_lookup->parse_status & kCanParse) &&
          (lookup_data->probability < structure_lookup->probability)) ||
         (!canParse &&
          (static_cast<int>(lookup_data->parse_status) <
           static_cast<int>(structure_lookup->parse_status))) ) {
      mpz_clear(lookup_data->index);
      delete lookup_data;
      lookup_data = structure_lookup;
      canParse = (lookup_data->parse_status & kCanParse);
    } else {
      // Clear up this structure's lookup data and don't update current data
      mpz_clear(structure_lookup->index);
      delete structure_lookup;
    }
  }

  // Before returning, fix the probability value
  lookup_data->probability = total_probability;
  return lookup_data;
}
