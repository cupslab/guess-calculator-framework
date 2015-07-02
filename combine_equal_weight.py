#!/usr/bin/python 
# Read two files of passwords that are in gzip word/freq format. The first file is the target and the second file will have the weights of its passwords increased to match the sum of frequencies of the target. Output is in gzip word/freq format.
#
# -*- coding: utf-8 -*-
# Written by SK


import sys, os.path, string, getopt, codecs, locale, gzip, subprocess
# from collections import defaultdict
reload(sys)
sys.setdefaultencoding("utf_8")
locale.setlocale(locale.LC_ALL,"")


def usage():
  print 'Usage: combine_equal_weight.py [OPTIONS]* <target corpus> <source corpus> [optional output file]'
  print ' -v, --verbose\tPrint out summary of work'
  print 'Help options:'
  print ' -?, -h, --help\tShow this help message'
  print ' --usage\tDisplay brief usage message'
  print
  print 'Inputs: gzipped files in word-freq format'
  print 'Output: The source file with weights adjusted to match target corpus in gzip word-freq format'
  print 'Output filename can be the same as the source filename if specified on the command-line as the third argument'
  print
  print 'Word-freq format:'
  print 'Password<tab>Frequency as float, weighted by weight<tab>Identifier'
  print
 


###############################################################################
# MAIN BLOCK
#


# Process command-line options
try:
  opts, args = getopt.gnu_getopt(sys.argv[1:], "?hv",["help","verbose","usage"])
except getopt.GetoptError, err:
  print str(err)
  usage()
  sys.exit(2)

verbose = 0
altout = False
for o,a in opts:
  if o in ('-v','--verbose'):
    verbose = 1
  elif o in ('-h','-?','--help','--usage'):
    usage()
    sys.exit(0)
  else:
    assert False, 'unhandled option'
if len(args) < 2:
  usage()
  sys.exit(2)
elif len(args) == 3:
  altout = True


# Open files and read
targetfile = args[0]
sourcefile = args[1]
try:
  targetcorpus = gzip.open(targetfile, 'rb')
  targetlines = targetcorpus.readlines()
except:
  sys.stderr.write("Failed to open " + targetfile + "!\n")
  sys.exit(2)
try:
  sourcecorpus = gzip.open(sourcefile, 'rb')
  sourcelines = sourcecorpus.readlines()
except:
  sys.stderr.write("Failed to open " + sourcefile + "!\n")
  sys.exit(2)

# Get sum of freqs for targetfile
targetsum = 0.0
for line in targetlines:
  line = line.rstrip('\r\n')
  line = line.encode('utf-8')
  fields = line.split("\t")
  incount = float.fromhex("0x" + fields[1])
  targetsum += incount

# Get sum of freqs for sourcefile
sourcesum = 0.0
for line in sourcelines:
  line = line.rstrip('\r\n')
  line = line.encode('utf-8')
  fields = line.split("\t")
  incount = float.fromhex("0x" + fields[1])
  sourcesum += incount

# Open output pipe
if altout:
  if verbose:
    sys.stderr.write("Using " + args[2] + " as output file...\n")  
  try:
    outfile = open(args[2],'wb')
  except:
    sys.stderr.write("Failed to open " + args[2] + "!\n")
    sys.exit(2)    
  oh = outfile
else:
  oh = sys.stdout
# Assign po to an object we can "write" to
p = subprocess.Popen("gzip -c", shell = True, stdin = subprocess.PIPE, stdout = oh)
po = p.stdin


# Now run through sourcefile and adjust weights
# No need to load a dictionary and resort, because the order of frequencies in the corpus will not change
weightadjust = targetsum / sourcesum
for line in sourcelines:
  line = line.rstrip('\r\n')
  line = line.encode('utf-8')
  fields = line.split("\t")
  inword = fields[0]
  incount = float.fromhex("0x" + fields[1])
  newcount = incount * weightadjust
  inID = fields[2]
  po.write(inword + "\t" + newcount.hex()[2:] + "\t" + inID + "\n")

# Flush pipe
p.communicate()

if verbose:
  sys.stderr.write('**********************************************\n')
  sys.stderr.write('* Summary:\n')
  sys.stderr.write('\tInput target: ' + targetfile + '\n')
  sys.stderr.write('\tInput source: ' + sourcefile + '\n')
  if altout:
    sys.stderr.write('\tOutput: ' + args[2] + '\n')
  else:
    sys.stderr.write('\tOutput: GZip to stdout\n')
  sys.stderr.write('\tTarget sum: ' + str(targetsum) + "\n")
  sys.stderr.write('\tSource sum: ' + str(sourcesum) + "\n")
  sys.stderr.write('\tWeight adjustment: ' + str(weightadjust) + "\n")
  sys.stderr.write('**********************************************\n')
