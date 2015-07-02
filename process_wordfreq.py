#!/usr/bin/python 
# Read a file of passwords, one per line, and convert to word/freq format
# If optional output argument is given, gzip the output using a subprocess (since this is faster than a python solution:  http://bugs.python.org/issue7471 )
#
# Input file is somewhat flexible:
# If file is already in gzip format, as identified by .gz extension, process with existing frequencies and IDs
#   If file is in gzip three-column format, assume the frequency is a float. Otherwise, assume frequency is an integer.
# If file is not gzipped, and -p is not specified, we assume the file is just passwords.
#   This is necessary because we have to split lines on tabs for word-freq format, and raw passwords might have tabs.
# Plaintext word-freq format files (like google_ascii_words_with_freqs.txt) must use the -p switch (or be gzipped before sending to this script).
#
# -*- coding: utf-8 -*-
# Written by SK


import sys, os.path, string, getopt, codecs, locale, gzip, subprocess
from collections import defaultdict
reload(sys)
sys.setdefaultencoding("utf_8")
locale.setlocale(locale.LC_ALL,"")


def usage():
  print 'Usage: process_wordfreq.py [OPTIONS]* <training corpus> [optional output file]'
  print ' -v, --verbose\tPrint out summary of training corpus'
  print ' -w, --weight\tAssign a weight to this source (default is 1). Options are parsed with getopt, so use -w 0.5 or --weight=0.5'
  print ' -g, --gzip\tPipe the output through gzip before outputting'
  print ' -p, --plaintext-input\tInput is in plaintext word-freq format'
  print ' -n, -r, --name, --replace-name\tReplace identifier for this source'
  print ' -a, --add-name\tAdd identifier for this source'
  print ' -m, --only-measure\tCalculate the total weight of the input corpus and output to stdout (no other output)'
  print ' -i, --ignore-breaks\tAllow \\001 character in input (this character is reserved for use as a structure break symbol)'
  print 'Help options:'
  print ' -?, -h, --help\tShow this help message'
  print ' --usage\tDisplay brief usage message'
  print
  print 'Word-freq format:'
  print 'Password<tab>Frequency as float, weighted by weight<tab>Identifier'
  print
  print 'NOTE: If <training corpus> has the .gz extension, it is assumed to be in word-freq format after gunzipping.'
  print




###############################################################################
# MAIN BLOCK
#


# Process command-line options
try:
  opts, args = getopt.gnu_getopt(sys.argv[1:], "?hvmipgn:r:a:w:",["help", "verbose", "only-measure", "ignore-breaks", "usage", "plaintext-input" "weight", "gzip", "name", "replace-name", "add-name"])
except getopt.GetoptError, err:
  print str(err)
  usage()
  sys.exit(2)

weight = 1.0
verbose = 0
measure = 0
bgzip = False
altout = False
replacename = False
wordfreq = False
allowstructurebreak = False
ID = ""
disallowed = ["\001", "\t"]
for o,a in opts:
  if o in ('-v','--verbose'):
    verbose = 1
  elif o in ('-m','--only-measure'):
    measure = 1
  elif o in ('-i','--ignore-breaks'):
    allowstructurebreak = True
    disallowed = ["\t"]
  elif o in ('-h','-?','--help','--usage'):
    usage()
    sys.exit(2)
  elif o in ('-w', '--weight'):
    weight = float(a)
    if verbose:
      sys.stderr.write("Using weight " + str(weight) + "\n")
  elif o in ('-g', '--gzip'):
    bgzip = True
  elif o in ('-p', '--plaintext-input'):
    wordfreq = True
  elif o in ('-a', '--add-name'):
    if not replacename:  # replacename overrides this option
      ID = a
      if verbose:
        sys.stderr.write("Using ID " + ID + "\n")
  elif o in ('-r', '-n', '--name', '--replacename'):
    replacename = True
    ID = a
    if verbose:
      sys.stderr.write("Using ID " + ID + "\n")
  else:
    assert False, 'unhandled option'
if len(args) < 1 or len(args) > 2:
  usage()
  sys.exit(2)
elif len(args) == 2:
  altout = True


# Determine extension of corpus file
bgunzip = False
if verbose:
  sys.stderr.write("Detected extension of " + os.path.splitext(args[0])[1] + " on input file.\n")
if os.path.splitext(args[0])[1] == '.gz':
  bgunzip = True
  wordfreq = True
# Read corpus file
if bgunzip:
  if verbose:
    sys.stderr.write("Assuming gzipped input file in word-freq format...\n")
  try:
    corpus = gzip.open(args[0],'rb')
  except:
    sys.stderr.write("Failed to open " + args[0] + "!\n")
    sys.exit(2)
else:
  try:
    corpus = open(args[0],'r')
  except:
    sys.stderr.write("Failed to open " + args[0] + "!\n")
    sys.exit(2)

# Read corpus line by line
line = u''
freqs = defaultdict(float)
IDs = {}
discardcount = 0
keptcount = 0
totalsum = 0.0
for line in corpus:
  line = line.rstrip('\r\n')
  line = line.encode('utf-8')  # Keep string in bytes representation

  # Skip blank lines in input -- no point wasting time processing them
  if len(line) < 1:
      continue
  
  inword = line
  incount = 1.0
  inID = ""
  if wordfreq:
    fields = line.split("\t")
    inword = fields[0]
    if len(fields) > 2:
      incount = float.fromhex("0x" + fields[1])  # hex float was stored without leading "0x"
      inID = fields[2]
    else:
      incount = int(fields[1])

  # Check if line contains a character that we don't want  
  if any(e in inword for e in disallowed):
    # Discard line
    if verbose:
      sys.stderr.write("Discarded line: " + line + "\n")
    discardcount += 1
    continue
  # Store count of line in dict with appropriate weight
  freqs[inword] += (weight * incount)
  totalsum += incount
  # Store ID in dict
  if inword in IDs:
    # Append ID if different
    curID = IDs[inword]
    if curID == "":
      IDs[inword] = inID
    else:
      curIDs = curID.split(",")
      inIDs = inID.split(",")  # inID might already be a list of IDs
      for curinID in inIDs:
        if not (curinID in curIDs):  # Add if curinID not already there
          IDs[inword] = curID + "," + curinID
  else:
    IDs[inword] = inID
  keptcount += 1

#####
##### If measure, we write the total sum to stdout and exit here.
#####
if measure:
  sys.stdout.write(str(totalsum) + "\n");
  sys.exit(0)


# Open output file - must wait until here to open output file in case it is the same as the input file
if altout:
  if verbose:
    sys.stderr.write("Using " + args[1] + " as output file...\n")  
  try:
    outfile = open(args[1],'wb')
  except:
    sys.stderr.write("Failed to open " + args[1] + "!\n")
    sys.exit(2)    
  oh = outfile
else:
  oh = sys.stdout

# Try to assign po to a single object we can "write" to
if bgzip:
  p = subprocess.Popen("gzip -c", shell = True, stdin = subprocess.PIPE, stdout = oh)
  po = p.stdin
else:
  po = oh

# Now iterate through dict and get the freqs. We don't need them in sorted order but I think it will be useful for other analyses.
totalsum = 0.0
topfreq = -1
for w in sorted(freqs, key=freqs.get, reverse=True):
  # Add or replace name
  if replacename:
    curID = ID
  else:
    curID = IDs[w]
    curIDs = curID.split(",")
    addIDs = ID.split(",")  # Added IDs might also be a list of IDs
    if ID != "":
      for curaddID in addIDs:
        if not (curaddID in curIDs):
          curID = curID + "," + curaddID
  totalsum += freqs[w]
  po.write(w + "\t" + freqs[w].hex()[2:] + "\t" + curID + "\n")
  # Save topfreq
  if topfreq == -1:
    topfreq = freqs[w]

if bgzip:
  p.communicate()

if verbose:
  sys.stderr.write('**********************************************\n')
  sys.stderr.write('* Summary of training corpus:\t' + args[0] + '\n')
  sys.stderr.write('\tApplied weight:\t' + str(weight) + "\n")
  sys.stderr.write('\tKept lines:\t' + str(keptcount) + "\n")
  sys.stderr.write('\tDiscarded lines:\t' + str(discardcount) + "\n")
  sys.stderr.write('\tFinal lines in dict:\t' + str(len(freqs)) + "\n")
  sys.stderr.write('\tLargest freq in dict:\t' + str(topfreq) + "\n")
  sys.stderr.write('\tInversion weight:\t' + '{:e}'.format(1/topfreq) + "\n")
  sys.stderr.write('\tSum of freqs in dict:\t' + str(totalsum) + "\n")
  sys.stderr.write('**********************************************\n')

