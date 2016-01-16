// nonterminal.cpp - a class for single nonterminals and their associated
//   production rules.
//
// Use of this source code is governed by the GPLv2 license that can be found
//   in the LICENSE file.
//
// Version 0.1
// Author: Saranga Komanduri
//   Based on code originally written and published by Matt Weir under the
//   GPLv2 license.
//
// Modified: Sat Jun 21 20:38:11 2014
//
// See header file for additional information

// Includes not covered in header file
#include <algorithm>
#include <cstdlib>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "grammar_tools.h"
#include "seen_terminal_group.h"
#include "unseen_terminal_group.h"

#include "nonterminal.h"


// Destructor for Nonterminal -- checks whether variables were initialized
// before deleting them
Nonterminal::~Nonterminal() {
  // Delete array of terminal groups
  if (terminal_groups_size_ > 0)
    for (unsigned int i = 0; i < terminal_groups_size_; ++i)
      delete terminal_groups_[i];
    delete[] terminal_groups_;
  // Unmap the terminal data file
  if (terminal_data_ != NULL)
    munmap(terminal_data_, terminal_data_size_);
}


// Init routine will use the given representation to find the correct terminals
// file in the terminals_folder and memory map it.  It will then make a pass
// through the terminal data to identify and store terminal group information
// in TerminalGroup objects.
//
// The terminals file must be found at:
// terminals_folder + terminal_representation_.txt
// where terminal_representation_ is representation with "U" replaced by "L"
// because terminals are all downcased when stored.
//
// Returns true on success
bool Nonterminal::loadNonterminal(const std::string& representation,
                                  const std::string& terminals_folder) {
  representation_ = representation;
  // Create "terminal" representation
  terminal_representation_ = representation;
  std::replace(terminal_representation_.begin(),
               terminal_representation_.end(),
               'U',
               'L');


  // Open the terminal file with open
  int file_handle;
  std::string terminal_filename =
    terminals_folder + terminal_representation_ + ".txt";
  file_handle = open(terminal_filename.c_str(), O_RDONLY | O_NONBLOCK);
  if (file_handle < 0) {
    int saved_errno = errno;
    perror("Error opening terminal file: ");
    fprintf(stderr,
      "Terminal filename: %s\nCalled for nonterminal represented by %s\n",
      terminal_filename.c_str(), representation.c_str());
    // Check if there error is too many open files in the process and output a
    // special error message
    if (saved_errno == EMFILE) {
      fprintf(stderr,
        "Error: Increase the open file limit of the OS. See INSTALL.md\n");
    }
    return false;
  }

  // Get file length
  struct stat file_statistics;
  if (fstat(file_handle, &file_statistics) < 0) {
    perror("Error getting file size: ");
    fprintf(stderr,
      "Terminal filename: %s\nCalled for nonterminal represented by %s\n",
      terminal_filename.c_str(), representation.c_str());
    return false;
  }
  // Make sure we can cast these types and set terminal_data_size
  if (sizeof(off_t) == sizeof(size_t)) {
    terminal_data_size_ = static_cast<size_t>(file_statistics.st_size);
  } else {
    fprintf(stderr,
      "Unable to cast off_t as size_t in LoadNonterminal!"
      " off_t has size %zu and size_t has size %zu!\n",
      sizeof(off_t), sizeof(size_t));
    return false;
  }

  // Memory map the terminal data file
  terminal_data_ =
    static_cast<char *>(mmap(static_cast<caddr_t>(0),
                             terminal_data_size_,
                             PROT_READ, MAP_SHARED, file_handle, 0));
  if (static_cast<void *>(terminal_data_) == MAP_FAILED) {
    perror("Error in memory mapping terminal data file: ");
    fprintf(stderr,
      "Terminal filename: %s\nCalled for nonterminal represented by %s\n",
      terminal_filename.c_str(), representation.c_str());
    return false;
  }

  // With temrinal data initialized, can now initialize terminal groups
  if (!initializeTerminalGroups())
    return false;

  return true;
}


// This function is called from loadNonterminal only.
//
// Parse the terminal data file and extract terminal groups, mostly using very
// low-level GNU C routines.
//
// Returns true on success
bool Nonterminal::initializeTerminalGroups() {

  // Get the number of terminal groups in the terminal_data
  // Set terminal_groups_size_ via a pass by reference
  if (!grammartools::CountTerminalGroupsInText(terminal_data_,
                                               terminal_data_size_,
                                               terminal_groups_size_)) {
    fprintf(stderr,
      "Counting terminal groups failed for nonterminal represented by %s\n!",
      representation_.c_str());
    return false;
  }

  terminal_groups_ = new TerminalGroup*[terminal_groups_size_];

  // Iterate over the data and initialize the groups
  // Keep track of group start pointers, group size, and group number
  char *data_position = terminal_data_;
  size_t bytes_remaining = terminal_data_size_;
  bool in_seen_groups = true;
  char *group_start = terminal_data_;
  uint64_t current_group_number = 0;
  mpz_t current_group_size;
  mpz_init_set_ui(current_group_size, 1);

  while (bytes_remaining > 0) {
    // Read the current line
    char read_buffer[1024];
    unsigned int bytes_read;
    grammartools::ReadLineFromCharArray(data_position, bytes_remaining,
                                        read_buffer, bytes_read);
    // If current line is blank, expect unseen terminals next and move forward
    if (bytes_read == 1) {  // Blank line = just the newline character was read
      in_seen_groups = false;

      // Skip to next line
      data_position += bytes_read;
      bytes_remaining -= bytes_read;
      continue;
    }

    // Parse the line
    std::string terminal, source_ids;
    double probability;
    if (!grammartools::ParseNonterminalLine(read_buffer, terminal,
                                            probability, source_ids)) {
      fprintf(stderr,
        "Line could not be parsed with read starting at byte %ld!\n",
        data_position - terminal_data_);
      return false;
    }

    // Determine if this is the last line of a group
    bool is_end_of_group;
    if (!grammartools::IsEndOfTerminalGroup(data_position, bytes_remaining,
                                            is_end_of_group)) {
      fprintf(stderr,
        "Parsing terminal data failed at byte %ld "
        "for nonterminal represented by %s\n!",
        data_position - terminal_data_,
        representation_.c_str());
      return false;
    }

    // If at the end of a group, initalize a new Terminal Group, else
    // increment the current group size.
    if (is_end_of_group) {
      if (in_seen_groups)
        terminal_groups_[current_group_number] =
          new SeenTerminalGroup(terminal_data_, probability,
                                current_group_size, representation_,
                                group_start,
                                data_position - group_start + bytes_read);
      else
        // For unseen groups, the source_ids field will contain a generator mask
        terminal_groups_[current_group_number] =
          new UnseenTerminalGroup(terminal_data_, probability,
                                  source_ids, representation_,
                                  terminal_data_size_);
      // Set group_start to the start of the next line
      group_start = data_position + bytes_read;
      ++current_group_number;
      mpz_set_ui(current_group_size, 1);
    } else {
      mpz_add_ui(current_group_size, current_group_size, 1);
    }

    // Move counters forward
    data_position += bytes_read;
    bytes_remaining -= bytes_read;
  }  // end while (bytes_remaining > 0)

  mpz_clear(current_group_size);
  return true;
}


// Iterate over terminal groups and return the total number of strings
void Nonterminal::countStrings(mpz_t result) const {
  mpz_init_set_ui(result, 0);

  for (uint64_t i = 0; i < terminal_groups_size_; ++i) {
    mpz_t string_count;
    terminal_groups_[i]->countStrings(string_count);
    mpz_add(result, result, string_count);
    mpz_clear(string_count);
  }
}


// Naively, this nonterminal *should* be able to produce any terminal that
// matches its string representation.  However, there is a slight mismatch
// because unseen terminals currently can only generate a restricted set of
// symbols, so we iterate over the terminal groups to make sure the given
// string can be produced.
//
TerminalLookupData* Nonterminal::lookup(const std::string& inputstring) const {
  TerminalLookupData *lookup_data = new TerminalLookupData;
  mpz_init(lookup_data->index);

  // First, check for a representation match
  std::string inputstring_representation;
  inputstring_representation.reserve(inputstring.size());
  for (unsigned int i = 0; i < inputstring.size(); ++i) {
    // Convert the inputstring to a USLD representation
    if( inputstring[i] >= 'a' && inputstring[i] <= 'z' )
      inputstring_representation.push_back('L');
    else if( inputstring[i] >= 'A' && inputstring[i] <= 'Z' )
      inputstring_representation.push_back('U');
    else if( inputstring[i] >= '0' && inputstring[i] <= '9' )
      inputstring_representation.push_back('D');
    else if( inputstring[i] == 1 )
      // If the \x01 character is found, give up
      return NULL;
    else
      inputstring_representation.push_back('S');
  }

  if (inputstring_representation != representation_ ) {
    lookup_data->parse_status = kTerminalNotFound;
    lookup_data->probability = -1;
    mpz_set_si(lookup_data->index, -1);
    return lookup_data;
  }

  // If representation matches, check over the terminal groups, but downcase
  // the string first.  The terminal groups' data is downcased (they can
  // match an out_representation, but terminal matching is to downcased
  // resources.)
  std::string downcased_string;
  downcased_string.resize(inputstring.size());
  std::transform(inputstring.begin(), inputstring.end(),
                 downcased_string.begin(), ::tolower);
  for (uint64_t i = 0; i < terminal_groups_size_; ++i) {
    // If index is not -1, then this terminal group can produce the input string
    LookupData *terminal_lookup = terminal_groups_[i]->lookup(downcased_string);

    if (terminal_lookup->parse_status & kCanParse) {
      // Copy terminal_lookup into lookup_data
      lookup_data->parse_status = kCanParse;
      lookup_data->probability = terminal_lookup->probability;
      mpz_set(lookup_data->index, terminal_lookup->index);
      lookup_data->source_ids.insert(terminal_lookup->source_ids.begin(),
                                     terminal_lookup->source_ids.end());
      lookup_data->terminal_group_index = i;

      // Free memory
      mpz_clear(terminal_lookup->index);
      delete terminal_lookup;

      return lookup_data;
    }

    mpz_clear(terminal_lookup->index);
    delete terminal_lookup;
  }
  // Otherwise, we never found a terminal group that can produce this string
  lookup_data->parse_status = kTerminalNotFound | kTerminalCantBeGenerated;
  lookup_data->probability = -1;
  mpz_set_si(lookup_data->index, -1);
  return lookup_data;
}


// Return true if this nonterminal can produce the given terminal.  This simply
// calls the lookup function to do the heavy lifting and returns true if the
// kCanParse status is returned.
bool Nonterminal::canProduceTerminal(const std::string& inputstring) const {
  TerminalLookupData *lookup_data = lookup(inputstring);
  if (lookup_data->parse_status == kCanParse) {
    mpz_clear(lookup_data->index);
    delete lookup_data;
    return true;
  }
  // else
  return false;
}


// Simple getter functions related to terminal groups
//
uint64_t Nonterminal::countTerminalGroups() const {
  return terminal_groups_size_;
}
//
// When returning values based on a terminal group index, die if the index
// is outside of the range of available terminal groups
//
std::string Nonterminal::getFirstStringOfGroup(uint64_t group_index) const {
  if (group_index >= terminal_groups_size_) {
    fprintf(stderr,
      "TerminalGroup index is outside of available range in "
      "getFirstStringOfGroup "
      "Asked for group_index of %ld and terminal_groups_size_ is %ld!\n",
      group_index, terminal_groups_size_);
    exit(EXIT_FAILURE);
  }

  return terminal_groups_[group_index]->getFirstString();
}


uint64_t Nonterminal::produceRandomTerminalGroup(std::mt19937 generator) const {
  std::uniform_real_distribution<double> distribution(0.0, 1.0);
  double prob = distribution(generator);
  for (uint64_t i = 0; i < terminal_groups_size_; i++) {
    mpz_t strings_in_group;
    mpf_t strings_in_group_f;
    mpf_t prob_in_group;
    mpz_init(strings_in_group);
    mpf_init_set_d(prob_in_group, getProbabilityOfGroup(i));
    mpf_init(strings_in_group_f);
    countStringsOfGroup(strings_in_group, i);
    mpf_set_z(strings_in_group_f, strings_in_group);
    mpf_mul(prob_in_group, prob_in_group, strings_in_group_f);
    prob -= mpf_get_d(prob_in_group);
    mpz_clear(strings_in_group);
    mpf_clear(strings_in_group_f);
    mpf_clear(prob_in_group);
    if (prob < 0) {
      return i;
    }
  }
  fprintf(stderr,
          "Error: Should not go here! Nonterminal::produceRandomTerminalGroup. "
          "Your randomness is all wrong!?!?!\n");
  return 0;
}


std::string Nonterminal::produceRandomStringOfGroup
(uint64_t group_index, std::mt19937 generator) const {
  mpz_t size;
  mpz_init(size);
  TerminalGroup* group = terminal_groups_[group_index];
  group->countStrings(size);
  // Go to size minus one because distribution has inclusive bounds
  uint64_t limit = mpz_get_ui(size) - 1;
  std::uniform_int_distribution<uint64_t> distribution(0, limit);
  mpz_clear(size);
  uint64_t random_item = distribution(generator);


  uint64_t counter = 0;
  std::string answer = "";
  TerminalGroup::TerminalGroupStringIterator* iterator =
    group->getStringIterator();
  while(!iterator->isEnd()) {
    if (counter == random_item) {
      answer = iterator->getCurrentString();
      break;
    }
    counter += 1;
    iterator->increment();
  }
  delete iterator;
  if (counter != random_item) {
    fprintf(stderr, "Error, Incorrect randomness in"
            " Nonterminal::produceRandomStringOfGroup?!?!?! "
            "Grammar directory is corrupt. Generated random string %" PRIu64
            " but saw only %" PRIu64 " elements. Returning empty string.\n",
            random_item, counter);
  }
  return answer;
}


//
double Nonterminal::getProbabilityOfGroup(uint64_t group_index) const {
  if (group_index >= terminal_groups_size_) {
    fprintf(stderr,
      "TerminalGroup index is outside of available range in "
      "getProbabilityOfGroup "
      "Asked for group_index of %ld and terminal_groups_size_ is %ld!\n",
      group_index, terminal_groups_size_);
    exit(EXIT_FAILURE);
  }

  return terminal_groups_[group_index]->getProbability();
}
//
void Nonterminal::countStringsOfGroup(mpz_t result, uint64_t group_index) const {
  if (group_index >= terminal_groups_size_) {
    fprintf(stderr,
      "TerminalGroup index is outside of available range in "
      "countStringsOfGroup "
      "Asked for group_index of %ld and terminal_groups_size_ is %ld!\n",
      group_index, terminal_groups_size_);
    exit(EXIT_FAILURE);
  }

  // countString will initalize result and set its value
  terminal_groups_[group_index]->countStrings(result);
}
//
TerminalGroup::TerminalGroupStringIterator*
  Nonterminal::getStringIteratorForGroup(
    uint64_t group_index) const {
  if (group_index >= terminal_groups_size_) {
    fprintf(stderr,
      "TerminalGroup index is outside of available range in "
      "getStringIteratorForGroup "
      "Asked for group_index of %ld and terminal_groups_size_ is %ld!\n",
      group_index, terminal_groups_size_);
    exit(EXIT_FAILURE);
  }

  return terminal_groups_[group_index]->getStringIterator();
}


// Simple getter function that returns a copy of the USLD representation for
// the nonterminal
std::string Nonterminal::getRepresentation() const {
  std::string representation(representation_);
  return representation;
}
