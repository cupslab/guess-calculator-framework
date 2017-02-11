// grammar_tools.cpp - a collection of miscellaneous, low-level functions
//   used when reading and parsing the grammar *on-disk* into higher-level 
//   objects
//
// Use of this source code is governed by the GPLv2 license that can be found
//   in the LICENSE file.
//
// Version 0.1
// Author: Saranga Komanduri
//   Based on code originally written and published by Matt Weir under the
//   GPLv2 license.
// 
// Modified: Fri May 30 18:35:30 2014
// 


// Includes not covered in header file
#include <cstdlib>
#include <cstring>
#include <sstream>

#include <string.h>
#include <unordered_map>

#include "grammar_tools.h"

namespace grammartools {

int CountLinesToNextBlankLine(FILE *fileptr) {
  // Store current position
  fpos_t oldposition;
  fgetpos(fileptr, &oldposition);

  // Count lines to next blank line
  char buf[1024];
  int linenum = 1, blanklinepos = 0;
  while (fgets(buf, 1024, fileptr) != NULL) {
    if ((strcmp(buf, "\n") == 0)) {
      blanklinepos = linenum;
      break;
    }
    ++linenum;
  }

  // Return to old position
  fsetpos(fileptr, &oldposition);

  // Return -1 on failure
  if (blanklinepos == 0) {
    // No blank line found
    return -1;
  }
  return blanklinepos;
}


int SkipStructuresHeader(FILE *fileptr) {
  char buf[1024];

  // The header is only one line and we can check it for correctness.
  if (fgets(buf, 1024, fileptr) != NULL) {
    if (strcmp(buf, "S ->\n") == 0) {
      return 1;
    }
  }

  // Return error if either of the above checks failed
  return -1;
}


// Read and parse a line from the structures file, checking for proper format.
// On failure, output the offending line to stderr.
bool ReadStructureLine(FILE *fileptr, 
                       std::string& structure, 
                       double& probability,
                       std::string& source_ids) {
  char buf[1024];
  char *tokstate;

  if (fgets(buf, 1024, fileptr) != NULL) {
    // Structure should be the string up to a tab character
    char *structureptr;
    structureptr = strtok_r(buf, "\t", &tokstate);
    if (structureptr == NULL) {
      goto error;  // strtok failed!
    } else {
      // Copy the c-string to the out-parameter using the 
      // std::string assignment operator
      structure = structureptr;
    }

    // Probability will be next
    char *probability_str;
    probability_str = strtok_r(NULL, "\t", &tokstate);
    if (probability_str == NULL) {
      goto error;  // strtok failed!
    } else {
      // Read in probability as a hex float and assign to out-parameter      
      probability = strtod(probability_str, NULL);

      // Check that probability is well-formed
      if (probability <= 0.0 || probability > 1.0) {
        goto error;
      }
    }

    // The remainder of the line will be source ids
    char *sourceidsptr;
    sourceidsptr = strtok_r(NULL, "\n", &tokstate);
    if (sourceidsptr == NULL) {
      goto error;  // strtok failed!
    } else {
      // Copy the c-string to the out-parameter using the 
      // std::string assignment operator
      source_ids = sourceidsptr;
    }
  } else {
    goto error;// fgets returned NULL
  }

  // Return success if all the above checks succeeded
  return true;

 error:
  fprintf(stderr, "Error in line: \"%s\" in structures file!\n", buf);
  return false;    
}

// Simple function to remove the \x01 character from the input string and
// return a new string
std::string StripBreakCharacterFromTerminal(const std::string& inputstring) {
  std::string unbroken;
  unbroken.reserve(inputstring.size());

  for (unsigned int i = 0; i < inputstring.size(); ++i) {
    if (inputstring[i] != 1)
      unbroken.push_back(inputstring[i]);
  }

  return unbroken;
}

// Given a source pointer of size source_length, return 
// length of a line
bool ReadLineFromCharArray2(const char *source,
                            unsigned int &size) {
  const char* end = strchr(source, '\n');

  if (end == NULL) {
    return false;
  }

  unsigned int length = (end - source) + 1;

  size = length;
  return true;
}
  
// Given a source pointer of size source_length, place one line from the
// source into the destination and provide the number of bytes read.
//
// Assumes line is less than 1024 bytes long (source_length can be any size)
//
// Returns true on success
bool ReadLineFromCharArray(const char *source, size_t source_length,
                           char *destination, unsigned int &bytes_read) {
  // Assume the line is less than 1024 bytes
  size_t bytes_to_read = 1023;
  bool end_of_string = false;
  if (source_length < 1024) {
    bytes_to_read = source_length;
    end_of_string = true;
  }
  
  // "Read" the characters by copying them into destination and check for errors
  char *temp_position = 
    static_cast<char *> (memccpy(destination, source, '\n', bytes_to_read));

  // Allow that the source might not end in a newline only if we consume the
  // whole source
  if (temp_position == NULL && !end_of_string) {
    fprintf(stderr,
      "Newline character not found with read where source_length was %zu\n",
      source_length);
    return false;    
  }

  // memccpy returns a pointer to just past the newline in destination unless
  // a newline was not found
  if (temp_position == NULL)
    bytes_read = bytes_to_read;
  else
    bytes_read = temp_position - destination;

  // Set the null character because memccpy won't
  destination[bytes_read] = '\0';

  return true;
}

// out parameter data for nonterminal line
struct pnldata {
  std::string terminal;
  double probability;
  std::string source_ids;
};

// Read a line from a source buffer, taken from a nonterminal file, and parse
// out the fields that are returned in the out-parameters.
// 
// NOTE: This function uses strtok which destroys that source buffer.
// 
// Return true on success, output the offending line to stderr on failure
bool ParseNonterminalLine(const char *source, const unsigned int length,
                          std::string& terminal, 
                          double& probability,
                          std::string& source_ids) {

  // do not reparse previously seen lines
  // protect if multithreaded
  static std::unordered_map<void *, struct pnldata> map;

  void * key = (void *)source;

  if (map.count(key) == 0) {
  // Tokenize the buffer using strtok
  char *terminalptr;
  char *tokstate;

  // get writable copy of string
  char *line = (char *)calloc(length + 1, 1);
  if (!line) {
    return false;
  }

  strncpy(line, source, length);
  

  terminalptr = strtok_r(line, "\t", &tokstate);
  if (terminalptr == NULL) {
    fprintf(stderr, "Terminal field not found!\n");
    return false;          
  }
  // Copy the c-string to the out-parameter using the 
  // std::string assignment operator
  terminal = terminalptr;

  char *probability_str;
  probability_str = strtok_r(NULL, "\t", &tokstate);
  if (probability_str == NULL) {
    fprintf(stderr, "Probability field not found!\n");
    return false;          
  }
  // Read in probability as a hex float and assign to out-parameter      
  probability = strtod(probability_str, NULL);
  // Check that probability is well-formed
  if (probability <= 0.0 || probability > 1.0) {
    fprintf(stderr, "Probability field not parsed correctly!\n");
    return false;                
  }

  // The remainder of the line will be source ids
  char *sourceidsptr;
  sourceidsptr = strtok_r(NULL, "\n", &tokstate);
  if (sourceidsptr == NULL) {
    fprintf(stderr,
      "Source IDs field not found!\n");
    return false;          
  }

  source_ids = sourceidsptr;

  struct pnldata value = {terminal, probability, source_ids};
  map.insert({key, value});

  free(line);
  return true;
  } else {
    auto data = map[key];
    terminal = data.terminal;
    probability = data.probability;
    source_ids = data.source_ids;

    return true;
  }
}


// Given a source pointer of size source_length, count the number of terminal
// groups in the data and place the result in an out parameter.
//
// Return true on success, return false on any failure to parse
bool CountTerminalGroupsInText(const char *source, size_t source_length,
                               uint64_t& number_of_groups) {
  // Count the number of terminal groups in the terminal_data
  const char *current_position = source;
  size_t bytes_remaining = source_length;
  number_of_groups = 0;
  double last_probability = 0.0;

  while (bytes_remaining > 0) {
    // Read a line from source into a buffer
    unsigned int bytes_read;
    // Note: bytes_read below is passed-by-reference (see declaration of
    // ReadLineFromCharArray)
    if (!grammartools::ReadLineFromCharArray2(current_position,
                                             bytes_read)) {
      fprintf(stderr,
        "Newline character not found with read starting at byte %ld!\n",
        current_position - source);
      return false;
    }
    if (bytes_read == 1) {
      // Blank line, so expect unseen terminals
      last_probability = 0.0;
      current_position += bytes_read;
      bytes_remaining -= bytes_read;
      continue;
    }

    // Get probability from the line so we can see if it is different from
    // the last probability seen
    std::string terminal, source_ids;
    double probability;
    if (!grammartools::ParseNonterminalLine(current_position, bytes_read, terminal,
                                            probability, source_ids)) {
      fprintf(stderr,
        "Line could not be parsed with read starting at byte %ld!\n",
        current_position - source);
      return false;          
    }

    // Check for a change in probability
    if (probability != last_probability)
      ++number_of_groups;

    // Update last_probability
    last_probability = probability;
    // Jump ahead the data pointer
    current_position += bytes_read;
    bytes_remaining -= bytes_read;
  }
  return true;
}


// Given a source pointer of size source_length, check if the current line is
// at the end of a terminal group.  The boolean is returned in an out parameter.
//
// The current line is at the end of a terminal group if:
// - The next line has a different probability from the current line
// - The next line is blank
// - There is a current line but no next line (end of source)
//
// If the current line is blank, this is not considered the end of a group.
//
// Return true on success, return false on any failure to parse
bool IsEndOfTerminalGroup(const char *source, size_t source_length,
                          bool& end_of_group) {
  unsigned int bytes_read;

  if (!ReadLineFromCharArray2(source, bytes_read)) {
    return false;
  }

  unsigned int firstlength = bytes_read;

  // Case: current line is blank -- this is not counted as an end of group
  if (bytes_read == 1) {
    end_of_group = false;
    return true;
  }

  // Case: no next line
  if ((source_length - bytes_read) == 0) {
    end_of_group = true;
    return true;
  }

  // Now that we know there is a next line, grab it
  const char *peek = source + bytes_read;
  if (!ReadLineFromCharArray2(peek, bytes_read)) {
    return false;
  }

  unsigned int peeklength = bytes_read;

  // Case: next line is blank
  if (bytes_read == 1) {
    end_of_group = true;
    return true;
  }

  // Case: next line has differing probability from current line
  // Parse both lines and extract the probabilities (ignore other fields,
  // but still return false on parse error)
  std::string terminal, source_ids;
  double current_probability, next_probability;
  if (!ParseNonterminalLine(source, firstlength, terminal,
                            current_probability, source_ids)) {
    return false;         
  }
  if (!ParseNonterminalLine(peek, peeklength, terminal,
                            next_probability, source_ids)) {
    return false;
  }
  end_of_group = (current_probability != next_probability);
  return true;
}


// Given a string of source_ids, which is a comma-separated list of values,
// parse out the values and add them to the given set
//
// Return true on success, false on any failure
bool AddSourceIDsFromString(const std::string& source_ids,
                            std::unordered_set<std::string>& source_list) {
  // Create stringstream from source and parse
  std::stringstream source_stream(source_ids);
  std::string source;
  while (std::getline(source_stream, source, ',')) {
    if (source.empty()) {
      fprintf(stderr,
        "String %s had an empty source value!\n",
        source_ids.c_str());
      return false;
    }
    source_list.insert(source);
  }

  // Free source_stream
  source_stream.clear();
  source_stream.str(std::string());
  return true;
}


} // namespace grammartools
