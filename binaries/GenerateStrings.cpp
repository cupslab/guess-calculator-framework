// GenerateStrings.cpp - a tool that loads a PCFG specification and generates
//   all strings whose probability is above a specified cutoff
//
// Use of this source code is governed by the GPLv2 license that can be found
//   in the LICENSE file.
//
// Version 0.2
// Author: Saranga Komanduri
//   Based on code originally written and published by Matt Weir under the
//   GPLv2 license.
//
// Modified: Sun Oct  5 21:22:21 2014
//

// The actual pattern generation is handled by the PCFG class.  Most of this
// file is concerned with parsing command-line arguments.
//
// The output consists of lines of the form:
// probability<tab>string
//
// The "true probabilities" option (-accupr) will work to some extent but it
// will *not* output all matching strings above the cutoff.  This is not possible,
// because there can always be a string with probability just below the cutoff
// that bounces above the cutoff when the probabilities of all of its tokenizations
// are accumulated.  It is not possible to find such strings without enumerating
// over all strings, which is simply not feasible.
//


#include <string>
#include <cstdio>
#include "pcfg.h"

void help() {
  printf("\n"
    "GenerateStrings - a tool for generating strings above a specified\n"
    "                  probability based on a learned PCFG\n"
    "Based on code originally written and published by Matt Weir\n"
    "  under the GPLv2 license.\n\n"
    "Author: Saranga Komanduri\n"
    "------------------------------------------------------------------------\n\n"
    "Usage Info:\n"
    "./GenerateStrings <options>\n"
    "\tOptions:\n"
    "\t-cutoff <probability>: Only output probability groups with values greater\n"
    "\t\tthan the given cutoff\n"
    "\t-accupr: (optional) Output true string probabilities for each guess by\n"
    "\t\tsumming over all tokenizations (note this is not needed if you have\n"
    "\t\ttokenized by character class because there is only one tokenization\n"
    "\t\tper string in that case\n"
    "\t-sfile <filename>: (optional) Use the following file as the structure file\n"
    "\t-tfolder <path>: (optional) Use the following folder as the terminals folder\n"
    "\t\tThis folder name MUST end in \"/\"\n"
    "\n\n\n");
  return;
}


int main(int argc, char *argv[]) {
  std::string structure_file = "grammar/nonterminalRules.txt";
  std::string terminal_folder = "grammar/terminalRules/";
  double cutoff = -1.0;
  bool accurate_probabilities = false;

  // Parse command-line arguments
  if (argc == 1) {
    help();
    return 0;
  }
  for (int i = 1; i < argc; ++i) {
    std::string commandLineInput = argv[i];

    if (commandLineInput.find("-cutoff") == 0) {
      ++i;
      if (i < argc) {
        sscanf(argv[i], "%le", &cutoff);
        if (cutoff > 1.0 || cutoff < 0) {
          fprintf(stderr, "\nError: the cutoff probability must fall "
                          "between 0 and 1.\n");
          help();
          return 1;
        }
      }
      else {
        fprintf(stderr, "\nError: no cutoff found after -cutoff option!\n");
        help();
        return 1;
      }

    } else if (commandLineInput.find("-accupr") == 0) {
      accurate_probabilities = true;

    } else if (commandLineInput.find("-sfile") == 0) {
      ++i;
      if (i < argc)
        structure_file = argv[i];
      else {
        fprintf(stderr, "\nError: no file found after -sfile option!\n");
        help();
        return 1;
      }

    } else if (commandLineInput.find("-sfile") == 0) {
      ++i;
      if (i < argc)
        terminal_folder = argv[i];
      else {
        fprintf(stderr, "\nError: no terminalfolder found after -sfile option!\n");
        help();
        return 1;
      }
    }
  }

  fprintf(stderr, "\nCutoff: %e\n"
                  "Using structure file: %s\n"
                  "Using terminal folder: %s\n\n",
                  cutoff, structure_file.c_str(), terminal_folder.c_str());

  PCFG pcfg;
  fprintf(stderr, "Begin loading PCFG specification...");
  pcfg.loadGrammar(structure_file, terminal_folder);
  fprintf(stderr, "done!\n");

  fprintf(stderr, "Begin generating strings...\n");
  if (pcfg.generateStrings(cutoff, accurate_probabilities))
    fprintf(stderr, "done!\n");
  else {
    fprintf(stderr, "\nError while generating strings!\n");
    return 1;
  }

  return 0;
}