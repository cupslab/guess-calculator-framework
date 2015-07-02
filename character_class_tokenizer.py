#!/usr/bin/python 
# Read a given file in gzipped word-freq format and tokenize the passwords
#   based on character class.  Tokenizing involves inserting \001 "break"
#   characters between character classes.
#
# The -d switch allows for duplicating strings so that both an untokenized
#   and tokenized version are produced.
#
# Output is in plaintext word-freq format to stdout.
#
# -*- coding: utf-8 -*-
# Original code written by Matt Weir
# Adapted by Saranga Komanduri
# Version: 0.2  Modified: Fri Sep  5 01:30:43 2014
#
import sys, os.path, string, codecs, locale, gzip, getopt
reload(sys)
sys.setdefaultencoding("utf_8")
locale.setlocale(locale.LC_ALL,"")

def usage():
	print 'Usage: character_class_tokenizer.py [OPTIONS]* <training corpus>'
	print ' -d x.y\tDuplicate strings so that tokenized and untokenized strings are both included.  In this case, the tokenzied string is assigned weight x.y times the untokenized string.'
	print ' -?, -h, --help\tShow this help message'
	print ' --usage\tDisplay brief usage message'
	print
	print 'NOTE: <training corpus> is assumed to be in gzipped word-freq format.'
	print


def tokenize_string(inputstring):
	# Tokenize the input by adding "\001" separator characters between class transitions
	structure = ''
	outputstring = ''
	prevclass = ""
	for x in range(0,len(inputstring)):
		curclass = ""
		if inputstring[x] in string.ascii_lowercase:
			curclass = "L"
		elif inputstring[x] in string.ascii_uppercase:
			curclass = "U"
		elif inputstring[x] in string.digits:
			curclass = "D"
		elif ord(inputstring[x])==1:
			curclass = "E"
		else:
			curclass = "S"
		# We know the input will never start or end with the separator character
		if (len(outputstring) > 0):
			if ( 
					((prevclass=="L" or prevclass=="U") and (curclass=="D" or curclass=="S")) or
					((prevclass=="S") and (curclass=="D" or curclass=="L" or curclass=="U")) or
					((prevclass=="D") and (curclass=="S" or curclass=="L" or curclass=="U"))
				 ):
				outputstring += '\001'  # Insert token separator before this character
		prevclass = curclass
		outputstring += inputstring[x]
	return outputstring


###############################################################################
# MAIN BLOCK
#


# Process command-line options
try:
  opts, args = getopt.gnu_getopt(sys.argv[1:], "?hd:",["help", "usage"])
except getopt.GetoptError, err:
  print str(err)
  usage()
  sys.exit(2)

bDuplicate = False
duplicateweight = 1.0
for o,a in opts:
	if o in ('-d'):
		bDuplicate = True
		duplicateweight = float(a)
	elif o in ('-h','-?','--help','--usage'):
		usage()
		sys.exit(0)
	else:
		assert False, 'unhandled option'
if len(args) <  1:
	usage()
	sys.exit(2)

# Unzip and open training corpus
try:
	corpus = gzip.open(args[0],'rb')
except:
	sys.stderr.write("Failed to open " + args[0] + "!\n")
	sys.exit(2)

# Read corpus line by line
line=u''
ctr = 0
pctr = 0
for line in corpus:
	line = line.rstrip('\r\n')
	#print line
	line = line.encode('utf-8')  # Keep string in bytes representation

	fields = line.split("\t")
	inword = fields[0]
	infreqstring = fields[1]
	infreq = float.fromhex("0x" + infreqstring)
	inID = fields[2]

	if bDuplicate:
		# If duplicate strings are desired, we first print out the untokenized string
		print (inword + "\t" + infreqstring + "\t" + inID)
		# Then the tokenized string with modified (typically lower) weight
		modfreq = infreq * duplicateweight
		tokenized_word = tokenize_string(inword)
		if (tokenized_word != inword):
			print (tokenized_word + "\t" + modfreq.hex()[2:] + "\t" + inID)
			pctr += 2
		else:
			pctr += 1
	else:
		# Only output the tokenized string
		tokenized_word = tokenize_string(inword)
		print (tokenized_word + "\t" + infreqstring + "\t" + inID)
		pctr += 1

	ctr += 1

sys.stderr.write("Tokenized " + str(ctr) + " lines and printed " + str(pctr) + " lines.\n")