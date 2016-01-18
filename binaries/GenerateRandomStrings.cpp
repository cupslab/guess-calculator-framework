// GenerateRandomStrings.cpp

#include <cstdio>
#include <inttypes.h>
#include <string>

#include "pcfg.h"
#include "randomness.h"

void help() {
    printf("\n"
           "GenerateRandomStrings - a tool that loads a PCFG and generates random\n"
           "                        strings whose probability is above a specified\n"
           "                        cutoff\n"
           "Usage Info\n"
           "\tOptions\n"
           "\t-p (Optional): Generate random patterns. This is more efficient but is not \n"
           "\t\tsuitable if you actually need strings. \n"
           "\t-number <integer>: Generate number of passwords\n"
           "\t\t summing over all tokenizations  (note this is not needed if you have\n"
           "\t\ttokenized by character class because there is only one tokenization\n"
           "\t\tper string in that case)\n"
           "\t-sfile <filename>: (optional) Use the following file as the structure file\n"
           "\t-tfolder <path>: (optional) Use the following folder as the terminals folder\n"
           "\t\tThis folder MUST end in \"/\"\n"
           "\t-seed (Optional): Seed the random number generator\n"
           "\n\n\n");
    return;
}


int main(int argc, char *argv[]) {
    std::string structure_file = "grammar/nonterminalRules.txt";
    std::string terminal_folder = "grammar/terminalRules/";
    uint64_t number = 0;
    uint64_t seed = 0;
    bool passed_seed = false;
    bool generate_patterns = false;

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
            } else {
                fprintf(stderr, "\nError no number after -number option\n");
                help();
                return 1;
            }
            if (number == 0) {
                fprintf(stderr,
                       "\nWarning, I was asked to generate 0 passwords\n");
            }
        } else if (commandLineInput.find("-p") == 0) {
            generate_patterns = true;
        } else if (commandLineInput.find("-seed") == 0) {
            ++i;
            if (i < argc) {
                sscanf(argv[i], "%" SCNu64, &seed);
                passed_seed = true;
            } else {
                fprintf(stderr, "\nError no number after -seed option\n");
                help();
                return 1;
            }
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
    if (generate_patterns) {
        fprintf(stderr, "Generating patterns\n");
    } else {
        fprintf(stderr, "Generating strings\n");
    }

    PCFG pcfg;
    fprintf(stderr, "Begin loading PCFG specification...");
    pcfg.loadGrammar(structure_file, terminal_folder);
    fprintf(stderr, "done!\n");

    fprintf(stderr, "Begin generating strings...\n");

    if (passed_seed) {
        fprintf(stderr, "Using seed %" PRIu64 "\n", seed);
    } else {
        std::random_device rd;
        seed = rd();
        fprintf(stderr, "Using randomly generated seed %" PRIu64 "\n", seed);
    }
    RNG mt_random_generator(seed);

    if (pcfg.generateRandomStrings
        (number, generate_patterns, &mt_random_generator)) {
        fprintf(stderr, "done!\n");
    } else {
        fprintf(stderr, "\nError while generating strings!\n");
        return 1;
    }
}
