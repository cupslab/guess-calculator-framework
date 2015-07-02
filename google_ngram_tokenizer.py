#!/usr/bin/python 
# -*- coding: utf-8 -*-
# Version: 0.81
# Author: Saranga Komanduri
# Modified: Fri Dec  5 22:39:00 2014
# 
# Take in a set of strings, one string per line from stdin and look them up in the token table
# See usage() for an explanation of the program
#
# This program has a large number of different modes that were used for various tasks in the testing phase.
#
# Todo: Make hybrid mode work for modes other than 28
#
# From v0.4+ this requires the marisa_trie cython library and a trie with keys for the token table

import sys, os, string, codecs, re, mmap
import locale
import marisa_trie
reload(sys)
sys.setdefaultencoding("utf_8")
locale.setlocale(locale.LC_ALL,"")

# Hard code path to token table for use in scripts
TOKENFILENAME = "/data/googlestrings/alphagrams.txt"
KEYSFILENAME = "/data/googlestrings/alphagramkeys.marisa"

# Optional arguments
tokenizedweight = 1.0
dupArg = ''

def usage():
  print "Usage: google_ngram_tokenizer.py [-d n] <mode> [path to token table]"
  print "  Take in a set of strings, one string per line from stdin and look them up in the token table"
  print 
  print "Arguments:"
  print "  -d n    = Tokenize with duplicating structures, with tokenized structures multiplied by the weight given in n"
  print "  -h n    = Tokenize with long hybrid structures, with tokenized structures multiplied by the weight given in n (only works with mode 28!)"
  print ""
  print "  mode: 1 = (raw) return all matches, one per line, with groups of matches separated by newlines"
  print
  print "  mode: 10 = (string format) take in strings, one per line, and return top match"
  print "  mode: 12 = (wordfreq string format) take in strings in plaintext wordfreq format, one per line, and return top match on single line (filter_strings should treat the space as a symbol and will properly filter from there)"
  print "  mode: 15 = (string format w/ extended tokenizing) take in strings, one per line, and return top match"
  print "  mode: 16 = (string format w/ slightly less greedy extended tokenizing) take in strings, one per line, and return top match"
  print "  mode: 17 = (wordfreq string format w/ extended tokenizing) take in strings in plaintext wordfreq format, one per line, and return top match on single line"
  print
  print "  mode: 20 = (training format) take in passwords, one per line, and insert \\001 characters between chunks INSIDE LETTER SEQUENCES ONLY. First separate string into runs of letters and non-letters and then tokenize runs of letters taking the top returned break."
  print "  mode: 22 = (wordfreq training format) take in passwords in plaintext wordfreq format, one per line, and insert \\001 characters between chunks INSIDE LETTER SEQUENCES ONLY."
  print "  mode: 25 = (training format w/ extended tokenizing) take in passwords, one per line, and insert \\001 characters between chunks INSIDE LETTER SEQUENCES ONLY."
  print "  mode: 27 = (wordfreq training format w/ extended tokenizing) take in passwords in plaintext wordfreq format, one per line, and insert \\001 characters between chunks INSIDE LETTER SEQUENCES ONLY."
  print "  mode: 28 = (wordfreq training format w/ charclass extended tokenizing) take in passwords in plaintext wordfreq format, one per line, and insert \\001 characters between chunks inside letter sequences AND break on character class (letter, digit, symbol)."
  print
  print "  mode: 30 = (test set format) take in passwords, which are the third column of tab-separated input, and tokenize as with mode 20, returning output in three-column format"
  print "  mode: 35 = (test set format w/ extended tokenizing) take in passwords, which are the third column of tab-separated input, and tokenize as with mode 25, returning output in three-column format"
  print
  print "  mode: 40 = (semantic) take in passwords, one per line, and insert \\001 characters between chunks inside letter sequences AND break on character class (letter, digit, symbol)."
  print "  mode: 45 = (semantic w/ extended tokenizing) take in passwords, one per line, and insert \\001 characters between chunks inside letter sequences AND break on character class (letter, digit, symbol)."
  print 
  sys.exit(1)


def binsearch(line):
  # Search the token table for the first argument and return the start and end positions of the key in the memory map
  #
  # tmm is the memory-mapped token table
  global tmm
  global tmmsize
  # Seek for line in tokenmm using modified-binary search
  #   Make sure to find the earliest match
  low = 0
  high = tmmsize - 1
  prev = -1  # Since we jump to the next newline before a check, to check the first key means we have to decrease med until it reaches zero! This can lead to unnecessary compares.
  savepos = -1
  endpos = -1
  cmpres = -1
  while low <= high:
    med = int((high + low) / 2)
    # scan to start of next line if not at beginning of file (otherwise you'll never find the first key)
    if med != 0:
      curpos = tmm.find('\n', med) + 1
    else:
      curpos = 0
    #print low, curpos, med, high, cmpres
    if curpos != -1:
      if curpos != prev:
        # Get this key
        prev = curpos
        tabpos = tmm.find('\t', curpos)
        key = tmm[curpos:tabpos]
        #print key
        cmpres = cmp(line, key)        
      if cmpres < 0:
        high = med - 1
      elif cmpres > 0:
        low = curpos
      else:
        # Found match! Now search backward
        while key == line:
          savepos = curpos
          #print "Found match at:", savepos
          curpos = tmm.rfind('\n', 0, curpos - 1) + 1
          if curpos != -1:  # -1 means we hit the beginning of the file
            tabpos = tmm.find('\t', curpos)
            key = tmm[curpos:tabpos]
          else:
            savepos = 0
            break            
        # Found match by going backward, now go forward to find the whole block
        key = line
        curpos = savepos
        while key == line:
          curpos = tmm.find('\n', curpos) + 1
          #print endpos, curpos
          if curpos == -1:
            endpos = high + 1
            break
          else:
            tabpos = tmm.find('\t', curpos)
            key = tmm[curpos:tabpos]
            endpos = curpos
        # Now have savepos and endpos, can stop scanning
        break
    else:  # Here means we had to scan past the end of the file
      savepos = -1
      endpos = -1
      break
  return (savepos, endpos)


def rawprint(startpos,endpos):
  # Return just the matches and their frequencies, with an ending blank newline
  #
  # tmm is the memory-mapped token table
  global tmm
  global tmmsize  

  if startpos == -1:
    print '-1\t-1'
  else:
    for row in tmm[startpos:endpos].rstrip('\n').split('\n'):  # rstrip the last newline, otherwise split has an empty strip as the last element
      print '\t'.join(row.split('\t')[1:3])
  print

def stringreturn(startpos,endpos,line):
  # Return just the top match with spaces between words
  #
  # tmm is the memory-mapped token table
  global tmm
  global tmmsize  

  if startpos == -1:
    return line
  else:
    tokens = tmm[startpos:endpos].rstrip('\n').split('\n')
    counts = map(lambda x: int(x.split('\t')[2]), tokens)
    # print counts
    bestpos = counts.index(max(counts))
    # take tokenization of best match
    besttok = tokens[bestpos].split('\t')[1]
    return besttok


def stringprint2(startpos,endpos,inword,infreq,inID):
  # Return just the top match with spaces between words
  #
  # tmm is the memory-mapped token table
  global tmm
  global tmmsize  

  if startpos == -1:
    print inword + "\t" + infreq + "\t" + inID
  else:
    tokens = tmm[startpos:endpos].rstrip('\n').split('\n')
    counts = map(lambda x: int(x.split('\t')[2]), tokens)
    # print counts
    bestpos = counts.index(max(counts))
    # take tokenization of best match
    besttok = tokens[bestpos].split('\t')[1]    
    print besttok + "\t" + infreq + "\t" + inID


def extendedtokenizestring(line, keepmode = 2):
  # Use the extended tokenizing algorithm to tokenize this string
  #
  # If keepmode == 1, keep all but the last token when tokenizing.
  # If keepmode == 2, keep only the first token tokenize forward with all remaining tokens. This is less greedy and more computationally intensive.
  #
  # keystrie is the marisa_trie with keys to the token table
  global keystrie

  # Process line iteratively
  brokenstring = ''
  lineremainder = line.decode('utf-8')  # marisa_trie requires all keys to be unicode
  while lineremainder != "":
    prefs = keystrie.prefixes(lineremainder)
    if len(prefs) > 0:
      lastpref = prefs[-1]  # The last element of the list will be the longest, so that's the one we want
      (startpos,endpos) = binsearch(lastpref)
      curbroken = stringreturn(startpos, endpos, lastpref)
      # If lastpref is the remainder of the string, we can stop here
      if lastpref == lineremainder:
        brokenstring += curbroken
        lineremainder = ''
      else:
        curwords = curbroken.split(" ")
        if len(curwords) < 2:
          # If we only have one token, there is no continuation possible, so add this and move on
          brokenstring += (curbroken + " ")
          lineremainder = lineremainder[len(curbroken):len(lineremainder)]
        else:
          # Take all but the last token and continue with the last token
          if keepmode == 1:
            tokeep = curwords[0:(len(curwords) - 1)]
            brokenstring += (" ".join(tokeep)) + " "
          elif keepmode == 2:
            tokeep = curwords[0]
            brokenstring += tokeep + " "  # if tokeep is a single word the join will insert spaces between every character
          lineremainder = lineremainder[len("".join(tokeep)):len(lineremainder)]
    else:
      sys.exit("No prefixes found in keys trie for " + lineremainder + " remainder of " + line)
  return brokenstring.encode('utf-8')  # return bytes


def tokenizepassword(line, tokenizermode = 0):
  # Take a password, chunk it, and tokenize the letter chunks
  #
  # tokenizermode = 0: normal tokenizing
  #               = 1: extended tokenizing
  #
  # There does not seem to be an efficient way to split the string in Python because regex ranges include Unicode. For safety, I parse it this way.
  outline = []
  curlet = 0
  startblock = 0
  if len(line) == 0:
    return line
  if line[0] in string.letters:
    curlet = 1
  for i in range(1, len(line) + 1):
    # Extra pass and logic used to process final chunk
    processnonletters = 0
    processletters = 0
    if i == len(line):  # range means i goes from 1 to len(line)
      if curlet == 0:          
        processnonletters = 1
      else:
        processletters = 1
    else:
      if (line[i] in string.letters) and curlet == 0:
        processnonletters = 1
      if not(line[i] in string.letters) and curlet == 1:
        processletters = 1
    if processnonletters == 1:
      # Just ended a block of nonletters
      outline.append(line[startblock:i])
      startblock = i
      curlet = 1
    elif processletters == 1:
      # Just ended a block of letters
      letchunk = line[startblock:i]      
      if tokenizermode == 0:
        (startpos,endpos) = binsearch(letchunk.lower())
        besttok = stringreturn(startpos, endpos, letchunk.lower())
      elif tokenizermode == 1:
        besttok = extendedtokenizestring(letchunk.lower())

      startblock = i
      curlet = 0
      # Get list of tokenizations
      # Break into tokens
      pieces = besttok.split(' ')
      # Now make array of slices of letchunk that correspond to the tokens found
      temp = []
      curpos = 0
      for piece in pieces:
        temp.append(letchunk[curpos:curpos + len(piece)])
        curpos += len(piece)
      # Now join the slices
      outline.append(string.join(temp, "\001"))
  # Compose output string and print
  return string.join(outline, '')



def charbreakpassword(n):
  # Take a password and insert break characters at character-class transitions
  # Code copied from process.py in calculator
  #
  ret = ''
  pw = ''
  for x in range(0,len(n)):
    if (len(ret) > 0):
      pc = nc  # Save the previous class      
    nc = ""
    if n[x] in string.ascii_lowercase:
      nc = "L"
    elif n[x] in string.ascii_uppercase:
      nc = "U"
    elif n[x] in string.digits:
      nc = "D"
    elif ord(n[x])==1:
      nc = "E"
    else:
      nc = "S"
    if (len(ret) > 0):
      if ( 
          ((pc=="L" or pc=="U") and (nc=="D" or nc=="S")) or
          ((pc=="S") and (nc=="D" or nc=="L" or nc=="U")) or
          ((pc=="D") and (nc=="S" or nc=="L" or nc=="U"))
          #(pc != "E") and (nc != "E") and (pc != nc)
         ):
        ret += "\001" + n[x]  # Insert break character "E" between every group
      else:
        ret += n[x]
    else:
      ret += n[x]
  return ret


#
# MAIN SECTION
#
# Parse command-line arguments
if len(sys.argv) < 2:
  usage()
elif len(sys.argv) == 2:  
  tmode = int(sys.argv[1])
  tokenfilename = TOKENFILENAME
elif len(sys.argv) == 4:
  tmode = int(sys.argv[3])
  tokenfilename = TOKENFILENAME
  dupArg = sys.argv[1]
  tokenizedweight = float(sys.argv[2])
elif len(sys.argv) == 3:
  tmode = int(sys.argv[1])  
  tokenfilename = sys.argv[2]
elif len(sys.argv) == 5:
  tmode = int(sys.argv[3])  
  tokenfilename = sys.argv[4]
  dupArg = sys.argv[1]
  tokenizedweight = float(sys.argv[2])
else:
  usage()

# Memory map the token file
sys.stderr.write('Memory-mapping token file...\n')
tokenfile = open(tokenfilename, "rb")  # Open file for read-only in binary mode
tmm = mmap.mmap(tokenfile.fileno(), 0, mmap.MAP_SHARED, mmap.PROT_READ)
tmmsize = tmm.size()
sys.stderr.write('Finished mapping file.\n')

# If this is an extended tokenizing mode, load the keys
if tmode in [15, 16, 17, 25, 27, 28, 35, 45]:
  sys.stderr.write('Loading keys trie...\n')
  keystrie = marisa_trie.Trie()
  keystrie.load(KEYSFILENAME)
  sys.stderr.write('Finished loading keys.\n')


# Process input from stdin
for line in sys.stdin:
  # Process each line
  line = line.rstrip('\r\n')
  line = line.encode('utf-8')  # Keep string in bytes representation
  if len(line) == 0:
    print
    continue
  # Duplicate input line if requested
  if dupArg == "-d":
    print line
  if tmode == 1:
    (startpos,endpos) = binsearch(line)
    # startpos should have the index or be -1
    #print line, "Savepos:", startpos, "Endpos:", endpos
    #print tmm[startpos:endpos]
    # Format output as desired
    rawprint(startpos,endpos)

  elif tmode == 10:
    (startpos,endpos) = binsearch(line)
    print stringreturn(startpos,endpos,line)
  elif tmode == 12:
    fields = line.split("\t")
    inword = fields[0]
    infreq = fields[1]
    # incount = float.fromhex("0x" + infreq)
    inID = fields[2]
    (startpos,endpos) = binsearch(inword)
    stringprint2(startpos, endpos, inword, infreq, inID)
  elif tmode == 15:
    print extendedtokenizestring(line, 1)
  elif tmode == 16:
    print extendedtokenizestring(line, 2)
  elif tmode == 17:
    fields = line.split("\t")
    inword = fields[0]
    infreq = fields[1]
    # incount = float.fromhex("0x" + infreq)
    inID = fields[2]
    print extendedtokenizestring(inword, 1) + "\t" + infreq + "\t" + inID



  elif tmode == 20:
    # Input is in training set format
    print tokenizepassword(line, 0)
  elif tmode == 22:
    fields = line.split("\t")
    inword = fields[0]
    infreq = fields[1]
    # incount = float.fromhex("0x" + infreq)
    inID = fields[2]
    print tokenizepassword(inword, 0) + "\t" + infreq + "\t" + inID
  elif tmode == 25:
    # Input is in training set format
    print tokenizepassword(line, 1)
  elif tmode == 27:
    fields = line.split("\t")
    inword = fields[0]
    infreq = fields[1]
    # incount = float.fromhex("0x" + infreq)
    inID = fields[2]
    print tokenizepassword(inword, 1) + "\t" + infreq + "\t" + inID
  elif tmode == 28:
    fields = line.split("\t")
    inword = fields[0]
    infreq = fields[1]
    incount = float.fromhex("0x" + infreq)
    modfreq = incount * tokenizedweight
    inID = fields[2]
    initialbreak = tokenizepassword(inword, 1)
    charbreak = charbreakpassword(initialbreak)
    if dupArg == "-h":
      # Use long hybrid structures
      charbreakweir = charbreakpassword(inword)
      if charbreakweir != charbreak:
        print charbreakweir + "\t" + infreq + "\t" + inID
    print charbreak + "\t" + modfreq.hex()[2:] + "\t" + inID


  elif tmode == 30:
    # Input is in test set format
    lineparts = line.split('\t')
    if len(lineparts) != 3:
      sys.exit('Input file not in three-column format!')
    lineparts[2] = tokenizepassword(lineparts[2], 0)
    print '\t'.join(lineparts)
  elif tmode == 35:
    # Input is in test set format
    lineparts = line.split('\t')
    if len(lineparts) != 3:
      sys.exit('Input file not in three-column format!')
    lineparts[2] = tokenizepassword(lineparts[2], 1)
    print '\t'.join(lineparts)

  elif tmode == 40:
    # Input is in training set format, but we want to break on character classes
    initialbreak = tokenizepassword(line, 0)  # first break inside letter chunks
    # Now break on character class transitions
    print charbreakpassword(initialbreak)
  elif tmode == 45:
    initialbreak = tokenizepassword(line, 1)
    print charbreakpassword(initialbreak)


# Close memory map and file
tmm.close()
tokenfile.close()
