# Makefile for guess calculator framework tools
#
# Use of this source code is governed by the GPLv2 license that can be found
#   in the LICENSE file.
#
# Version 0.1
# Author: Saranga Komanduri
#
CC = g++
CFLAG =-O0 -g -std=c++11
# Enable the following options (and add \ to the above line) for more warnings
# -pedantic -Wall -Wextra -Wcast-align -Wcast-qual -Wctor-dtor-privacy -Wdisabled-optimization \
# -Wformat=2 -Winit-self -Wlogical-op -Wmissing-declarations -Wmissing-include-dirs -Wnoexcept \
# -Wold-style-cast -Woverloaded-virtual -Wredundant-decls -Wshadow -Wsign-conversion -Wsign-promo \
# -Wstrict-null-sentinel -Wstrict-overflow=5 -Wswitch-default -Wundef

CLASSFILES=bit_array.* gcfmacros.* grammar_tools.* lookup_data.* lookup_tools.* mixed_radix_number.* \
           nonterminal_collection.* \
           nonterminal.* pcfg.* pattern_manager.* seen_terminal_group.* structure.* \
           terminal_group.* unseen_terminal_group.*

CLASS_CPP_FILES = grammar_tools.cpp lookup_tools.cpp mixed_radix_number.cpp \
           nonterminal_collection.cpp nonterminal.cpp pcfg.cpp pattern_manager.cpp seen_terminal_group.cpp \
           structure.cpp unseen_terminal_group.cpp
CLASS_OBJ_FILES = $(CLASS_CPP_FILES:.cpp=.o)

default: main

main: GeneratePatterns sortedcountaggregator LookupGuessNumbers GenerateStrings GenerateRandomStrings

# Binaries must be compiled with the GMP library
GeneratePatterns: GeneratePatterns.o
	$(CC) $(CFLAG) $(CLASS_OBJ_FILES) GeneratePatterns.o -o GeneratePatterns -lgmpxx -lgmp

GeneratePatterns.o: GeneratePatterns.cpp .classes
	$(CC) $(CFLAG) -c GeneratePatterns.cpp

GenerateStrings: GenerateStrings.o
	$(CC) $(CFLAG) $(CLASS_OBJ_FILES) GenerateStrings.o -o GenerateStrings -lgmpxx -lgmp

GenerateStrings.o: GenerateStrings.cpp .classes
	$(CC) $(CFLAG) -c GenerateStrings.cpp

GenerateRandomStrings: GenerateRandomStrings.o
	$(CC) $(CFLAG) $(CLASS_OBJ_FILES) $< -o $@ -lgmpxx -lgmp

GenerateRandomStrings.o: GenerateRandomStrings.cpp .classes
	$(CC) $(CFLAG) -c $<

LookupGuessNumbers: LookupGuessNumbers.o
	$(CC) $(CFLAG) $(CLASS_OBJ_FILES) LookupGuessNumbers.o -o LookupGuessNumbers -lgmpxx -lgmp

LookupGuessNumbers.o: LookupGuessNumbers.cpp .classes
	$(CC) $(CFLAG) -c LookupGuessNumbers.cpp


.classes: $(CLASSFILES)
	$(CC) $(CFLAG) -c $(CLASS_CPP_FILES)
	touch .classes

sortedcountaggregator: sortedcountaggregator.o
	$(CC) $(CFLAG) sortedcountaggregator.o -o sortedcountaggregator -lgmpxx -lgmp

sortedcountaggregator.o: sortedcountaggregator.cpp
	$(CC) $(CFLAG) -c sortedcountaggregator.cpp

clean:
	rm -f GeneratePatterns
	rm -f sortedcountaggregator
	rm -f LookupGuessNumbers
	rm -f GenerateStrings
	rm -f GenerateRandomStrings
	rm -f .classes
	rm -f *.o
