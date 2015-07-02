// GeneratePatterns.cpp - a tool that loads a PCFG specification and generates
//   all patterns whose probability is above a specified cutoff
//
// Use of this source code is governed by the GPLv2 license that can be found
//   in the LICENSE file.
//
// Version 0.1
// Author: Saranga Komanduri
//   Based on code originally written and published by Matt Weir under the
//   GPLv2 license.
//
// Modified: Thu Jul 24 10:45:36 2014
//

// The actual pattern generation is handled by the PCFG class.  Most of this
// file is concerned with parsing command-line arguments.
//

#include <string>
#include <cstdio>
#include "pcfg.h"

void help() {
  printf("\n"
    "GeneratePatterns - a tool for generating patterns above a specified\n"
    "                   probability based on a learned PCFG\n"
    "Based on code originally written and published by Matt Weir\n"
    "  under the GPLv2 license.\n\n"
    "Author: Saranga Komanduri\n"
    "------------------------------------------------------------------------\n\n"
    "Usage Info:\n"
    "./GeneratePatterns <options>\n"
    "\tOptions:\n"
    "\t-cutoff <probability>: Only output probability groups with values greater\n"
    "\t\tthan the given cutoff\n"
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

  fprintf(stderr, "Begin generating patterns...\n");
  if (pcfg.generatePatterns(cutoff))
    fprintf(stderr, "done!\n");
  else {
    fprintf(stderr, "\nError while generating patterns!\n");
    return 1;
  }

  return 0;
}