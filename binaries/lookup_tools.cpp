// lookup_tools.cpp - a collection of miscellaneous, low-level functions
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
#include <fcntl.h>
#include <sys/stat.h>

#include "lookup_tools.h"

namespace lookuptools {

// Given an istream containing lines of a password file in lookup-format,
// parse out the password and original line and return in out-parameters.
//
bool ReadPasswordLineFromStream(std::ifstream& passwordFile,
                                std::string& fullline,
                                std::string& password) {
  // By default, this will extract until the '\n' and return false if
  // getline fails (EOF)
  if (!std::getline(passwordFile, fullline))
    return false;

  // The line should contain three tab-separated fields and we want the index
  // of the second tab to extract the password.
  unsigned int tab_counter = 0, tab2_index = 0;
  for (unsigned int i = 0; i < fullline.size(); ++i) {
    if (fullline[i] == '\t') {
      ++tab_counter;
      if (tab_counter == 2)
        tab2_index = i;
    }
  }

  if (tab_counter != 2) {
    fprintf(stderr, "Error: password line: \"%s\" does not contain three "
                    "tab-separated fields!\n", fullline.c_str());
    return false;
  }

  password = fullline.substr(tab2_index + 1, fullline.size());
  return true;
}


// Read and parse a line from the lookup table file, checking for proper format.
// On failure, output the offending line to stderr.
bool ReadLookupTableLine(FILE *fileptr,
                         double& probability,
                         std::string& guess_number,
                         std::string& pattern_string) {
  char buf[1024];
  if (fgets(buf, 1024, fileptr) != NULL) {

    // Process probability
    char *probability_str;
    probability_str = strtok(buf, "\t");
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

    // Process guess number next
    char *guess_number_ptr;
    guess_number_ptr = strtok(NULL, "\t");
    if (guess_number_ptr == NULL) {
      goto error;  // strtok failed!
    } else {
      // Copy the c-string to the out-parameter using the
      // std::string assignment operator
      guess_number = guess_number_ptr;
    }

    // The remainder of the line will be the pattern string
    char *pattern_string_ptr;
    pattern_string_ptr = strtok(NULL, "\n");
    if (pattern_string_ptr == NULL) {
      goto error;  // strtok failed!
    } else {
      // Copy the c-string to the out-parameter using the
      // std::string assignment operator
      pattern_string = pattern_string_ptr;
    }
  } else {
    goto error;// fgets returned NULL
  }

  // Return success if all the above checks succeeded
  return true;

 error:
  fprintf(stderr, "Error in line: \"%s\" in lookup table file!\n", buf);
  return false;
}


// Given a file pointer, use fseek to go back in characters until we have gone
// back one line.  The goal is to make the next fgets call return a valid "line"
// which will end in a '\n'. If we RewindOneLine after an fgets call, the next
// fgets call should return the same result, i.e., we should return to the same
// position (assuming the first fgets call was at the start of a line).
//
// To deduce the proper operation of this function, I considered the following
// cases:
// - If the current character is '\n' and the previous character is '\n',
//   rewind past the previous character to the previous '\n' and then go
//   forward one character.
// - If the current character is '\n' and the previous character is not '\n',
//   we are not in a valid state, i.e., after an fgets call we would never be
//   sitting on a newline, so just rewind to the previous '\n' and go forward
//   one character.
// - If the current character is not '\n' and the previous character is '\n',
//   we are in the state one would expect after an fgets call.  Rewind past
//   two newlines, then go forward one character.
// - If the current character is not '\n' and the previous character is not '\n',
//   we are not in a valid state.  Rewind to the previous '\n' and go forward
//   one character.
//
// We can then simplify the above cases to a much simpler algorithm:
// - skip back 2 from the current position, then keep rewinding until a '\n'
//   is found, and have the file pointer be at the next character after that
//   '\n'
//
// If rewinding gets us to the start of the file, call rewind() and return false.
// The call to rewind is because otherwise the file pointer might be in an
// indeterminate state after a failed fseeko.
//
// Note: The return value of false is only useful if you are certain that you
// should not be close to the begininng of the file when RewindOneLine is called.
// This is done in the FindLastProbability function, where fseeko is used to
// go to the end of the file.  A failure here might indicate that fseeko is not
// implemented correctly on your system.
//
bool RewindOneLine(FILE *lookupFile) {
  // Implement the specified algorithm
  if (fseeko(lookupFile, -2, SEEK_CUR) != 0) {
    rewind(lookupFile);
    return false;
  }

  while (fgetc(lookupFile) != '\n') {
    // Skip back two each time, since fgetc moved the pointer forward one
    if (fseeko(lookupFile, -2, SEEK_CUR) != 0) {
      rewind(lookupFile);
      return false;
    }
  }
  return true;
}


// Get the last (lowest) probability from the lookup file.  We do this by
// seeking to the end of the file and then rewinding until we read the last
// probability.
//
// The last probability should be in the second-to-last line of the file.
// While there, we also check that the last line of the file is well-formed,
// i.e., it starts with a T (for "Total count").
//
// Die on any unexpected error.
//
double FindLastProbability(FILE *lookupFile) {
  if (fseeko(lookupFile, -1, SEEK_END) != 0) {
    perror("Error seeking to end of lookup table file: ");
    exit(EXIT_FAILURE);
  }

  // The last line of the file should be a Total count line
  if (!RewindOneLine(lookupFile)) {
    perror("Error rewinding in lookup table file: ");
    exit(EXIT_FAILURE);
  }
  if (fgetc(lookupFile) != 'T') {
    fprintf(stderr, "Error: Lookup table file does not seem to have a last line"
                    " starting with \"Total count\"!\n");
    exit(EXIT_FAILURE);
  }

  // Now go back one character to undo the fgetc and rewind another line
  fseeko(lookupFile, -1, SEEK_CUR);
  if (!RewindOneLine(lookupFile)) {
    perror("Error rewinding in lookup table file: ");
    exit(EXIT_FAILURE);
  }
  // Make sure this line starts with a probability
  if (fgetc(lookupFile) != '0') {
    char buf[1024];
    fgets(buf, 1024, lookupFile);
    fprintf(stderr, "Error: Lookup table file does not seem to have a "
                    "probability on its second to last line! "
                    "Instead, found %s\n",
                    buf);
    exit(EXIT_FAILURE);
  }
  fseeko(lookupFile, -1, SEEK_CUR);

  // Parse the probability and return
  // Need to use strtod and this requires a char * -- a file stream won't work
  char buf[1024];
  fgets(buf, 1024, lookupFile);
  return strtod(buf, NULL);
}


// Perform a binary search on the lookup table file.  The search is slightly
// complicated by the fact that random-access is at the byte level rather
// than at the record level.  It is more complicated by the fact that the
// given key can match multiple entries.  We want to return the first of these.
//
// The algorithm is as follows:
// - low, high, mid, and midpos are of type off_t which indicates a position
// in the input file
//
// Invariant: a) Probability at RewindOneLine(low) >= key
//            b) Probability at RewindOneLine(high) <= key
// - low = start, high = (seek to file size - 1; RewindOneLine; subtract 1)
// - Check invariant: kUnexpectedFailure if a) not met,
//                    kBeyondCutoff if b) not met
// while (low <= high)
// - mid = (high - low)/2 + low
// - midpos = RewindOneLine(mid)
// - if ((Probability at midpos) == key)
//   - if (midpos > 0 &&
//         (Probability at RewindOneLine(midpos - 1) ==
//         (Probability at midpos)
//     - high = midpos - 1
//   - else
//     - seek to midpos
//     - return kCanParse
// - else
//   - if ((Probability at midpos) > key)
//     - low = ForwardOneLine(midpos)
//   - else
//     - high = midpos - 1
// - kUnexpectedFailure
//
// Return a ParseStatus type to indicate the status of the search.  On success,
// return kCanParse and expect file position to be at the beginning of a block
// of matching lines, else return an error status as shown above.
//
// Die on a number of file operation errors.
//
ParseStatus BinarySearchLookupTable(FILE *lookupFile, double key) {
  off_t low = 0;
  // To set high marker, seek to end and then back up one line because
  // the last line is a "Total count" line
  if (fseeko(lookupFile, -1, SEEK_END) != 0) {
    perror("Error seeking to end of lookup table file: ");
    exit(EXIT_FAILURE);
  }
  if (!RewindOneLine(lookupFile)) {
    exit(EXIT_FAILURE);
  }
  off_t high = ftello(lookupFile) - 1;
  if (high < 0) {
    perror("Error getting position of file from ftello: ");
    exit(EXIT_FAILURE);
  }

  // Check invariant (a)
  if (fseeko(lookupFile, low, SEEK_SET) != 0) {
    perror("Error seeking to \'low\' position in lookup table file: ");
    exit(EXIT_FAILURE);
  }
  double read_probability;
  std::string guess_number, pattern_string;
  if (!ReadLookupTableLine(lookupFile, read_probability, guess_number, pattern_string)) {
    fprintf(stderr, "Unable to read probability from lookup table file!\n");
    exit(EXIT_FAILURE);
  }
  if (read_probability < key) {
    // Input probability is higher than any probability in the lookup table!
    return kUnexpectedFailure;
  }

  // Check invariant (b)
  if (fseeko(lookupFile, high, SEEK_SET) != 0) {
    perror("Error seeking to \'high\' position in lookup table file: ");
    exit(EXIT_FAILURE);
  }
  if (!RewindOneLine(lookupFile)) {
    exit(EXIT_FAILURE);
  }
  if (!ReadLookupTableLine(lookupFile, read_probability, guess_number, pattern_string)) {
    fprintf(stderr, "Unable to read probability from lookup table file!\n");
    exit(EXIT_FAILURE);
  }
  if (read_probability > key) {
    // Input probability is lower than any probability in the lookup table!
    return kBeyondCutoff;
  }

  // Start binary search
  while (low <= high) {
    off_t mid = ((high - low) / 2) + low;

    // Move to and record midpos
    if (fseeko(lookupFile, mid, SEEK_SET) != 0) {
      fprintf(stderr, "Tried seeking to position %jd\n",
                      static_cast<intmax_t>(mid));
      perror("Error seeking to mid in lookup table file: ");
      exit(EXIT_FAILURE);
    }
    RewindOneLine(lookupFile);
    off_t midpos = ftello(lookupFile);
    if (midpos < 0) {
      perror("Error getting position of file from ftello: ");
      exit(EXIT_FAILURE);
    }

    // Get probability at midpos
    double midpos_probability;
    if (!ReadLookupTableLine(lookupFile, midpos_probability, guess_number, pattern_string)) {
      fprintf(stderr, "Unable to read probability from lookup table file!\n");
      exit(EXIT_FAILURE);
    }

    if (midpos_probability == key) {
      bool matches_previous = false;
      if (midpos > 0) {  // If no previous entry, nothing to check

        // We are searching for the case where the previous entry does not match
        // the key
        if (fseeko(lookupFile, midpos - 1, SEEK_SET) != 0) {
          fprintf(stderr, "Tried seeking to position %jd\n",
                          static_cast<intmax_t>(midpos - 1));
          perror("Error seeking to midpos in lookup table file: ");
          exit(EXIT_FAILURE);
        }
        RewindOneLine(lookupFile);

        // Get the probability at previous entry
        double previous_probability;
        if (!ReadLookupTableLine(lookupFile, previous_probability, guess_number, pattern_string)) {
          fprintf(stderr, "Unable to read probability from lookup table file!\n");
          exit(EXIT_FAILURE);
        }

        // Check the two values
        if (previous_probability == midpos_probability) {
          matches_previous = true;
          high = midpos - 1;
        }
      }

      if (!matches_previous) {
        // Found a case where we match the key and the previous line does
        // not match the key.  Set the file pointer to midpos and return.
        fseeko(lookupFile, midpos, SEEK_SET);
        return kCanParse;
      }
    } else {
      // Bisect the search space based on midpos_probability.  Remember that
      // the table is in descending sorted order.
      if (midpos_probability > key) {

        // Go to mid and go ahead one line
        fseeko(lookupFile, midpos, SEEK_SET);
        char buf[1024];
        fgets(buf, 1024, lookupFile);  // fgets will consume a line
        low = ftello(lookupFile);

      } else {
        high = midpos - 1;
      }
    }
  }

  // If we never found a match, return failure
  return kUnexpectedFailure;
}



// Given a FILE pointer to a lookup table, and two keys: a probability and
// a pattern string, return a LookupData object where parse_status represents
// the search success and index is the rank of this pattern, i.e., guess number
// as found in the lookup table.
//
// Note that unlike the way ranks are computed everywhere else in the guess
// calculator framework, the guess number is one-indexed rather than zero-indexed.
// This is because it represents a count, rather than an abstract rank or index.
//
LookupData *TableLookup(FILE *lookupFile, const double probability,
                        const std::string& patternkey) {
  LookupData *lookup_data = new LookupData;
  mpz_init_set_si(lookup_data->index, -1);
  mpz_init_set_si(lookup_data->next_index, -1);

  // If the probability is lower than anything in the lookup table, return
  static double lowest_probability = FindLastProbability(lookupFile);
  if (probability < lowest_probability) {
    lookup_data->parse_status = kBeyondCutoff;
    return lookup_data;
  }

  // Move the file pointer to the first probability in the file that matches
  ParseStatus return_status = BinarySearchLookupTable(lookupFile, probability);
  if (!(return_status & kCanParse)) {
    // Key was not found!
    lookup_data->parse_status = return_status;
    return lookup_data;
  }

  // Now check for the pattern key among the matching lines -- there can be
  // multiple patterns with the same probability
  double read_probability = probability;
  double next_probability = probability;
  std::string guess_number, pattern_string, next_pattern, next_guess_number;
  while (read_probability == probability) {
    // Guard against reading the "Total count" line
    char peek_character = static_cast<char>(fgetc(lookupFile));
    if (peek_character == 'T')
      break;
    // else undo the fgetc and read and parse the line
    fseeko(lookupFile, -1, SEEK_CUR);
    if (!ReadLookupTableLine(lookupFile, read_probability,
                             guess_number, pattern_string)) {
      fprintf(stderr, "Unable to parse values from line in lookup table file!\n");
      exit(EXIT_FAILURE);
    }

    if (patternkey == pattern_string) {

      // Read another line to see the next pattern guess number
      char peek_character = static_cast<char>(fgetc(lookupFile));
      if (peek_character != 'T') {
        fseeko(lookupFile, -1, SEEK_CUR);
        if (!ReadLookupTableLine(lookupFile, next_probability,
                                 next_guess_number, next_pattern)) {
          fprintf(stderr, "Unable to parse values from line in lookup table file!\n");
          exit(EXIT_FAILURE);
        }
        mpz_set_str(lookup_data->next_index, next_pattern.c_str(), 10);
      }

      // Found match!
      mpz_set_str(lookup_data->index, guess_number.c_str(), 10);
      lookup_data->parse_status = kCanParse;
      return lookup_data;
    }
  }

  // If here, a pattern key match was not found
  lookup_data->parse_status = kUnexpectedFailure;
  return lookup_data;
}


} // namespace lookuptools
