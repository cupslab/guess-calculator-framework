#include <string>
#include <fstream>

#include "pcfg.h"
#include "lookup_data.h"
#include "lookup_tools.h"

void help() {
  printf("\n"
         "LookupProbabilties - a tool that loads a PCFG and looks up the \n"
         "probabilities for each password, or assigns a code that explains \n"
         "why the password was not found\n"
         "\tOptions\n"
         "\t-pfile <filename>: a password file in three-column, tab-separated format\n"
         "\t-gdir <directory>: a \"grammar directory\" produced by the calculator\n"
         "\n\n");
  return;
}

int main(int argc, char *argv[]) {
  std::string default_structure_file = "grammar/nonterminalRules.txt";
  std::string default_terminal_folder = "grammar/terminalRules/";
  std::string structure_file;
  std::string terminal_folder;
  std::string password_file;
  std::string grammar_dir = "grammar/";
  if (argc != 3 && argc != 5) {
    help();
    return 0;
  }
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
    }
  }
  if (password_file == "") {
    fprintf(stderr, "Password file and not specified!\n");
    help();
    return 1;
  }
  if (structure_file.empty())
    structure_file = default_structure_file;
  if (terminal_folder.empty())
    terminal_folder = default_terminal_folder;
  fprintf(stderr, "\nReading password file: %s\n"
          "Using structure file: %s\n"
          "Using terminal folder: %s\n\n",
          password_file.c_str(),
          structure_file.c_str(), terminal_folder.c_str());
  PCFG pcfg;
  fprintf(stderr, "Begin loading PCFG specification...");
  pcfg.loadGrammar(structure_file, terminal_folder);
  fprintf(stderr, "done!\n");

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

    double probability = -1.0;
    // If the password was parsed, search for it in the lookup table
    if (lookup_data->parse_status & kCanParse) {
      probability = lookup_data->probability;
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
    } else {
      probability = static_cast<double>(-(lookup_data->parse_status));
    }

    // Print all of the source ids that went into this guess
    std::string final_source_ids = "";
    for (auto it = lookup_data->source_ids.begin();
         it != lookup_data->source_ids.end();
         ++it) {
      final_source_ids.append(*it);
    }

    // Output a line to stdout
    printf("%s\t%a\t%s\t%s\n",
           fullline.c_str(),  // Original line from passwords file
           probability,
           lookup_data->first_string_of_pattern.c_str(),
           final_source_ids.c_str());
    mpz_clear(lookup_data->index);
    delete lookup_data;
  }

  return 0;
}
