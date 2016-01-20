// LookupGuessNumbers.cpp - a tool that loads a PCFG specification and lookup
//   table and determines guess numbers for each password found, or assigns a
//   code based on why the password was not found.
//
// Use of this source code is governed by the GPLv2 license that can be found
//   in the LICENSE file.
//
// Version 0.1
// Author: Saranga Komanduri
//   Based on code originally written and published by Matt Weir under the
//   GPLv2 license.
//
// Modified: Fri Jul 25 12:04:43 2014
//

// The main tools of this code are the data structures in lookup_data.h,
// the functions in lookup_tools.*, and the lookup methods of various
// objects in the guess calculator framework.
//
// The lookup algorithm is roughly as follows. For each password in the input
// password file:
// - Call PCFG::lookup on the password.  This will iterate over the structure
//   objects and call Structure::lookup on the password.
//   - Structure::lookup breaks the password into terminals (returning if the
//     password cannot be parsed by this structure) and calls
//     PatternManager::lookup on the array of terminals.
//     - PatternManager is required because the password might be part of a
//       permutable pattern, and the PatternManager class is the only one that
//       can rank and unrank permutations.
//     - PatternManager will return the "rank" of the password in its pattern,
//       a pattern-identifying string, and the probability of the password. It
//       calls the structures Terminal Groups to index each of the terminals,
//       which return a special code if a terminal is not in the context-free
//       grammar.
// - Each structure can only parse a password in one or zero ways, but multiple
//   structures could parse a given password.  Return the best parse among all
//   structures, i.e., the one with lowest guess number.
// - Now we look up the probability and pattern-identifying string in the input
//   lookup table file.  This file is sorted by decreasing probability, so we
//   use binary search.
// - If found, the lookup table file will give the guess number of the first
//   string produced by that pattern.  Since we have a zero-indexed rank from
//   PatternManager, we just add this to the guess number from the lookup table
//   to get the true guess number of the password.
// - This is printed to stdout, along with other diagnostic values.
//

#include <string>
#include <cstdio>
#include <cstdlib>
#include <fstream>

#include "pcfg.h"
#include "lookup_data.h"
#include "lookup_tools.h"

void help() {
  printf("\n"
    "LookupGuessNumbers - a tool that loads a PCFG specification and lookup\n"
    "                     table and determines guess numbers for each password\n"
    "                     found, or assigns a code that explains why the\n"
    "                     password was not found\n"
    "Based on code originally written and published by Matt Weir\n"
    "  under the GPLv2 license.\n\n"
    "Author: Saranga Komanduri\n"
    "------------------------------------------------------------------------\n\n"
    "Usage Info:\n"
    "./LookupGuessNumbers <options> <optional options>\n"
    "\tOptions:\n"
    "\t-pfile <filename>: a password file in three-column, tab-separated format\n"
    "\t-lfile <filename>: a lookup table file in sorted, aggregrated-count format\n"
    "\tOptional Options:\n"
    "\t-gdir <directory>: a \"grammar directory\" produced by the calculator\n"
    "\t-bias-up (Optional): bias the guess numbers toward 0 on probability tie\n"
    "\t-bias-down (Optional): bias the guess numbers away from 0 on probability tie\n"
    "\n\n\n");
  return;
}


int main(int argc, char *argv[]) {
  std::string default_structure_file = "grammar/nonterminalRules.txt";
  std::string structure_file;
  std::string default_terminal_folder = "grammar/terminalRules/";
  std::string terminal_folder;
  std::string password_file;
  std::string lookup_file;
  std::string grammar_dir;

  // This value controls what lookup numbers are printed when there are ties in
  // probability (ie. two guesses have the same probability). By default we
  // return an exact value, meaning a model would actually make that many
  // guesses. We can optionally bias away from 0 (optimistic) or away from 0
  // (pessimistic).
  // -1 means bias towards 0, 1 means bias away from 0, 0 means no bias
  int bias_index = 0;

  for (int i = 1; i < argc; ++i) {
    std::string commandLineInput = argv[i];
    if (commandLineInput.find("-pfile") == 0) {
      ++i;
      if (i < argc)
        password_file = argv[i];
      else {
        fprintf(stderr, "\nError: no file found after -pfile option!\n");
        help();
        return 1;
      }
    } else if (commandLineInput.find("-lfile") == 0) {
      ++i;
      if (i < argc)
        lookup_file = argv[i];
      else {
        fprintf(stderr, "\nError: no file found after --lfile option!\n");
        help();
        return 1;
      }
    } else if (commandLineInput.find("-gdir") == 0) {
      ++i;
      if (i < argc) {
        grammar_dir = argv[i];
        if (grammar_dir.back() != '/') {
          grammar_dir += '/';
        }
        structure_file = grammar_dir + "nonterminalRules.txt";
        terminal_folder = grammar_dir + "terminalRules/";
      }
      else {
        fprintf(stderr, "\nError: no directory found after --gdir option!\n");
        help();
        return 1;
      }
    } else if (commandLineInput.find("-bias-up") == 0) {
      bias_index = 1;
    } else if (commandLineInput.find("-bias-down") == 0) {
      bias_index = -1;
    }
  }
  if (password_file == "" || lookup_file == "") {
    fprintf(stderr, "Password file and/or lookup table file not specified!\n");
    help();
    return 1;
  }
  if (structure_file.empty())
    structure_file = default_structure_file;
  if (terminal_folder.empty())
    terminal_folder = default_terminal_folder;

  fprintf(stderr, "\nReading password file: %s\n"
                  "Using lookup table file: %s\n"
                  "Using structure file: %s\n"
                  "Using terminal folder: %s\n\n",
                  password_file.c_str(), lookup_file.c_str(),
                  structure_file.c_str(), terminal_folder.c_str());


  PCFG pcfg;
  fprintf(stderr, "Begin loading PCFG specification...");
  pcfg.loadGrammar(structure_file, terminal_folder);
  fprintf(stderr, "done!\n");

  if (bias_index != 0) {
    if (bias_index == 1) {
      fputs("Biasing numbers away from zero\n", stderr);
    } else {
      fputs("Biasing numbers toward zero\n", stderr);
    }
  }

  // Open lookup table for random access
  FILE *lookupFile = fopen(lookup_file.c_str(), "rb");
  if (lookupFile == 0) {
    fprintf(stderr, "Error opening file: %s!\n", lookup_file.c_str());
    exit(EXIT_FAILURE);
  }

  // Open password file for reading line-by-line
  fprintf(stderr, "Begin parsing password file...\n");
  std::ifstream passwordFile(password_file);
  if (!passwordFile.is_open()) {
    fprintf(stderr, "Error opening file: %s!\n", password_file.c_str());
    exit(EXIT_FAILURE);
  }


  // Begin lookups -- grab passwords from password file
  std::string fullline, password;
  while (lookuptools::ReadPasswordLineFromStream(passwordFile,
                                                 fullline, password)) {
    // Lookup password using PCFG
    LookupData *lookup_data = pcfg.lookup(password);

    // If the password was parsed, search for it in the lookup table
    if (lookup_data->parse_status & kCanParse) {
      LookupData *table_lookup =
        lookuptools::TableLookup(lookupFile,
                                 lookup_data->probability,
                                 lookup_data->first_string_of_pattern);
      if (table_lookup->parse_status & kCanParse) {
        // Password was found!  Add the value in the lookup table to the
        // rank of the password in its pattern
        if (bias_index == 0) {
          mpz_add(lookup_data->index, lookup_data->index, table_lookup->index);
        } else if (bias_index == 1) {
          mpz_set(lookup_data->index, table_lookup->next_index);
        } else {
          mpz_set(lookup_data->index, table_lookup->index);
        }
      } else {
        // If the password was parsed, but not found in the lookup table,
        // the only acceptable reason for is kBeyondCutoff
        if (table_lookup->parse_status & kBeyondCutoff) {
          lookup_data->parse_status = kBeyondCutoff;
        } else {
          fprintf(stderr, "Failed to find parseable password in lookup table!\n"
                          "Should have found password: %s with probability: %a "
                          "and pattern_string: %s but failed!\n",
                          password.c_str(),
                          lookup_data->probability,
                          lookup_data->first_string_of_pattern.c_str());
          exit(EXIT_FAILURE);
        }
      }
      mpz_clear(table_lookup->index);
      mpz_clear(table_lookup->next_index);
      delete table_lookup;
    } else if ((lookup_data->parse_status & kTerminalCollision) ||
               (lookup_data->parse_status & kUnexpectedFailure)) {
      fprintf(stderr, "Password lookup returns unexpected error code! "
                      "Something went horribly wrong!\n"
                      "Attempting to parse password: %s with probability: %a "
                      "and pattern_string: %s but returned parse code: "
                      "-%d when such codes should not be produced!\n",
                      password.c_str(),
                      lookup_data->probability,
                      lookup_data->first_string_of_pattern.c_str(),
                      static_cast<unsigned>(lookup_data->parse_status));
      exit(EXIT_FAILURE);
    }

    // Set up strings for printing to stdout
    // Set up guess number string or print the parse_status in the guess
    // number field (with negative value) as a diagnostic
    char final_guess_number[1024];
    if (lookup_data->parse_status & kCanParse) {
      mpz_get_str(final_guess_number, 10, lookup_data->index);
    } else {
      sprintf(final_guess_number, "-%d",
              static_cast<unsigned>(lookup_data->parse_status));
      lookup_data->first_string_of_pattern = "";
    }

    // Print all of the source ids that went into this guess
    std::string final_source_ids = "";
    for (auto it = lookup_data->source_ids.begin();
              it != lookup_data->source_ids.end();
              ++it) {
      final_source_ids.append(*it);
    }

    // Output a line to stdout
    printf("%s\t%a\t%s\t%s\t%s\n",
           fullline.c_str(),  // Original line from passwords file
           lookup_data->probability,
           lookup_data->first_string_of_pattern.c_str(),
           final_guess_number,
           final_source_ids.c_str());
    mpz_clear(lookup_data->index);
    delete lookup_data;
  }

  fclose(lookupFile);
  return 0;
}
