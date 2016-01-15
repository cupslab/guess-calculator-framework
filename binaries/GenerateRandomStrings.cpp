// GenerateRandomStrings.cpp

#include <cstdio>
#include <inttypes.h>
#include <string>

#include "pcfg.h"

void help() {
    printf("\n"
           "GenerateRandomStrings - a tool that loads a PCFG and generates random\n"
           "                        strings whose probability is above a specified\n"
           "                        cutoff\n"
           "Usage Info\n"
           "\tOptions\n"
           "\t-number <integer>: Generate number of passwords\n"
           "\t-accupr: (optional) Output true string probabilityes for each guess by\n"
           "\t\t summing over all tokenizations  (note this is not needed if you have\n"
           "\t\ttokenized by character class because there is only one tokenization\n"
           "\t\tper string in that case)\n"
           "\t-sfile <filename>: (optional) Use the following file as the structure file\n"
           "\t-tfolder <path>: (optional) Use the following folder as the terminals folder\n"
           "\t\tThis folder MUST end in \"/\"\n"
           "\n\n\n");
    return;
}


int main(int argc, char *argv[]) {
    std::string structure_file = "grammar/nonterminalRules.txt";
    std::string terminal_folder = "grammar/terminalRules/";
    bool accurate_probabilities = false;
    uint64_t number = 0;

    // Parse command-line arguments
    if (argc == 1) {
        help();
        return 0;
    }

    for (int i = 1; i < argc; ++i) {
        std::string commandLineInput = argv[i];
        if (commandLineInput.find("-number") == 0) {
            ++i;
            if (i < argc) {
                sscanf(argv[i], "%" SCNu64, &number);
            }
            if (number == 0) {
                fprintf(stderr,
                       "\nWarning, I was asked to generate 0 passwords\n");
            }
        } else if (commandLineInput.find("-accupr") == 0) {
            accurate_probabilities = true;
        } else if (commandLineInput.find("-sfile") == 0) {
            ++i;
            if (i < argc)
                structure_file = argv[i];
            else {
                fprintf(stderr,
                        "\nError: no file found after -sfile option!\n");
                help();
                return 1;
            }
        } else if (commandLineInput.find("-tfolder") == 0) {
            ++i;
            if (i < argc)
                terminal_folder = argv[i];
            else {
                fprintf(stderr,
                        "\nError: no terminalfolder found after"
                        " -tfolder option!\n");
                help();
                return 1;
            }
        }
    }

    fprintf(stderr, "\nNumber: %" PRId64 "\n"
            "Using structure file: %s\n"
            "Using terminal folder: %s\n\n",
            number, structure_file.c_str(), terminal_folder.c_str());

    PCFG pcfg;
    fprintf(stderr, "Begin loading PCFG specification...");
    pcfg.loadGrammar(structure_file, terminal_folder);
    fprintf(stderr, "done!\n");

    fprintf(stderr, "Begin generating strings...\n");
    if (pcfg.generateRandomStrings(number, accurate_probabilities))
        fprintf(stderr, "done!\n");
    else {
        fprintf(stderr, "\nError while generating strings!\n");
        return 1;
    }
}
