#!/usr/bin/python 
# Read in a corpus in gzipped word-freq format and construct a PCFG specification.
#   A specification consists of a file with structures (productions of the start
#   symbol) and a directory of files with terminals (productions of nonterminals)
#   where each file corresponds to all productions of a single nonterminal.
# Terminals are only learned in lowercase, with capitalization performed at the
#   nonterminal level.  For example, if UL is a nonterminal, it will reference the
#   LL file, which contains lowercase strings like at, on, etc.  When the actual
#   production is applied by the UL nonterminal, the strings produced will be At,
#   On, etc.
#
# As of version 0.8, whole strings are taken as terminals, unless the \001
#   character is found within a string.  In this case, the \001 character separates
#   terminals, and each terminal is taken with the frequency of the original string.
#
# -*- coding: utf-8 -*-
# Original code written by Matt Weir
# Heavily modified by Saranga Komanduri
#
# Version 0.85  Modified: Fri Sep  5 13:02:31 2014

import sys, os.path, string, getopt, codecs, locale, gzip
from collections import defaultdict
reload(sys)
sys.setdefaultencoding("utf_8")
locale.setlocale(locale.LC_ALL,"")

# Constants
STRUCTUREBREAKCHAR = "E"
MAXSTRUCTURELENGTH = 40  # Must match the kMaxStructureLength constant in pcfg.h
												 # Also should match any similar config value used when tokenizing

# Input switches
verbose = 0
bStructures = True
bTerminals = True
bUnseenTerminals = False

# Item -> frequency maps
structure_groups = defaultdict(float)
terminal_groups = defaultdict(float)
# Item -> source maps
structure_sets = defaultdict(set)
terminal_sets = defaultdict(set)

def usage():
	print 'Usage: process.py [OPTIONS]* <training corpus>'
	print ' -v, --verbose\tPrint out summary of training corpus'
	print ' -u, --unseenterminals\tUse Simple Good-Turing smoothing to estimate probability for unseen terminals'
	print ' -t, --noterminals\tDon\'t learn terminals'
	print ' -g, --nostructures\tDon\'t learn structures'
	print
	print 'Help options:'
	print ' -?, -h, --help\tShow this help message'
	print ' --usage\tDisplay brief usage message'
	print
	print 'Word-freq format:'
	print 'Password<tab>Frequency as hex float with leading "0x" removed, weighted by weight<tab>Identifier'
	print
	print 'NOTE: <training corpus> is assumed to be in gzipped word-freq format.'
	print


def get_structure(inputstring):
	# Return the "structure" of the input, which is a mapping of the input to
	#   character classes, with "E" separator symbols added wherever a token
	#   separator exists
	structure = ''
	for x in range(0,len(inputstring)):
		curclass = ""
		if inputstring[x] in string.ascii_lowercase:
			curclass = "L"
		elif inputstring[x] in string.ascii_uppercase:
			curclass = "U"
		elif inputstring[x] in string.digits:
			curclass = "D"
		elif ord(inputstring[x])==1:
			curclass = STRUCTUREBREAKCHAR
		else:
			curclass = "S"
		structure += curclass
	return structure

def structure_combinations(structure):
	# Given a structure, return the number of terminals this structure could
	# produce.
	combinations = 1
	for c in structure:
		if c == "L" or c == "U":
			combinations *= 26
		elif c == "D":
			combinations *= 10
		elif c == "S":
			# The number below is a little rough, but it should match
			#   the number of characters in the kGeneratorSymbols string
			#   in unseen_terminal_group.cpp.
			combinations *= 33
		elif c == "E":
			# "pass" is a "do nothing" statement in Python
			pass
		else:
			sys.stderr.write("Found invalid character in structure: " + structure + "!\n")
			sys.exit(2)			
	return combinations

def store_structure(m, freq, ID):
	# Add the string m to the structures dict and update IDs associated with m
	structure_groups[m] += freq
	curids = ID.split(",")
	for curid in curids:
		structure_sets[m].add(str(curid))

def sort_group(g):
	# Return dict reverse sorted by value (highest values first) *with keys and values reversed!*
	g = [(v,k) for k,v in g.items()]  # dict is indexed by freq
	g.sort()
	g.reverse()
	return g

def group_keys(grp):
	# Return set that contains nonterminal "keys" for the strings in the group
	# We use get_structure to generate the keys
	keys = set()  # Use set to automatically ignore collisions
	for k in grp.keys():
		tmp = get_structure(k)
		keys.add(tmp)
	return keys

def group_sorted_keys(sorted_grp):
	# Return hash from nonterminal "keys" to terminals in the group
	# sorted_grp is a sorted list of (freq, terminal) tuples and we preserve
	#   the sorted order in the returned lists so that each list in the returned
	#   hash is itself a sorted group that is the subset of sorted_grp matching
	#   a particular structure.
	# We use get_structure to generate the keys
	structuremap = dict()
	for freq, terminal_string in sorted_grp:
		structure = get_structure(terminal_string)
		if structure in structuremap:
			structuremap[structure].append( (freq, terminal_string) )
		else:
			structuremap[structure] = [ (freq, terminal_string) ]
	return structuremap

def extract_terminals(inword, freq, ID):
	# Extract terminals from the word, seperating if the break character is
	# encountered.  This function operates via side-effects, adding to the
	# terminal_groups and terminal_sets objects.
	#
	# Note: we only learn lowercase terminals.  Uppercase characters are 
	# encoded in the structure.
	structure = get_structure(inword)
	word = inword.lower()
	if len(word) != len(structure):
		sys.stderr.write("Structure " + structure + " is not the same length as word " + word + "!\n" +
										 "This should never happen!\n")
		sys.exit(2)
	t = []
	val = ''
	for x in range(0,len(word)):
		if(structure[x] != STRUCTUREBREAKCHAR):
			val += word[x]
		else:
			if(val != ''):
				t.append(val)
				val = ''
	if(val != ''):
		t.append(val)
	# t now contains all terminals found in the word
	for y in range(0,len(t)):
		terminal_groups[t[y]] += freq
		curids = ID.split(",")
		for curid in curids:
			terminal_sets[t[y]].add(str(curid))
	return t


def calculate_total(grp):
	# Return sum of freqs (float) for given group (dict)
	total = 0.0
	for count,value in grp:
		total += count
	return total

def process_structures():
	# Write structure file in sorted order of frequency, calculating probability for each
	sorted_structures = sort_group(structure_groups)
	totalcount = calculate_total(sorted_structures)  	# Sum total freqs
	fd = open('grammar/nonterminalRules.txt','w')
	# Start the file with productions from the start symbol
	fd.write("S ->\n")
	ctr = 0
	for count, micro in sorted_structures:  # structures is already sorted by value
		f4 = float(count)/totalcount
		hexprob = f4.hex()  # Print as hexfloat to preserve original bits
		myids = (','.join(structure_sets[micro]))  # Join set elements with commas
		fd.write(micro + "\t" + hexprob + "\t" + myids + "\n")
		ctr += 1
	# Since this concludes rules that start with the start symbol, add a blank line
	#   to end this section
	fd.write("\n")
	fd.close()
	#print "  Found %d structures" % ctr

def sum_over_terminals(sorted_grp):
	# Given a sorted group, return the total sum of freqs for the group.
	total = 0.0
	for freq, terminal_string in sorted_grp:
		total += freq
	return total

def sum_over_singletons(sorted_grp, structure):
	# Given a sorted group, return the total sum of freqs for the group that have
	#   minimum frequency.  These would be the "singletons" or items that only 
	#   appeared once.  Since the corpora are weighted, the true frequency is lost,
	#   but we can assume that elements with the most common frequency are singletons
	#   (this might be a poor assumption.)
	# 
	# Also returns the minimum frequency encountered, and a rough estimate of the
	#   number of unseen terminals (only rough because of ungeneratable symbols).
	#
	# Find most common frequency
	counts = defaultdict(int)
	totalcount = 0
	for freq, terminal_string in sorted_grp:
		counts[str(freq)] += 1
		totalcount += 1
	freqs = [float(i) for i in list(counts.keys())]
	freqsoffreqs = list(counts.values())
	indexofmostcommon = freqsoffreqs.index(max(freqsoffreqs))
	minfreq = min(freqs)

	# Total frequency mass of most common frequency
	totalmostcommon = freqs[indexofmostcommon] * freqsoffreqs[indexofmostcommon]

	# Make sure totalcount is less than what this structure can generate,
	#   otherwise there are no unseen terminals.
	k = structure_combinations(structure)
	if totalcount < k:
		return (totalmostcommon, minfreq, k - totalcount)
	else:
		return (0, 0, 0)

def good_turing(sorted_grp,structure):
	# Given a sorted group and structure, return the unseen probability mass and a
	#   total sum of freqs for elements of the group that match this structure,
	#   with the total sum adjusted so that *seen* frequencies, when divided 
	#   by this value are adjusted to make room for the unseen mass.
	#
	# It is assumed that the values of the sorted_grp match the given structure.
	#   If this assumption is violated, the function will surely fail.
	#
	# This function uses something like a Good-Turing estimator for unseen
	# 	probability mass, though the unseen mass assigned is typically far
	# 	lower than would be correct under Good-Turing, especially for extreme
	# 	cases where all of the seen terminals would qualify as having been
	# 	seen once.  In these cases, the seen terminals are assigned
	# 	probability 0.5 and the unseen mass another 0.5, even though the
	# 	unseen mass should be assigned something as close to one as possible.
	# 	This is because we require that unseen terminals have individual
	# 	probability lower than any seen terminal, due to the need to have
	# 	terminals in decreasing order, and the fact that unseen terminals
	# 	always go at the very end of terminal files.  In the future, a more
	# 	sophisticated technique might be implemented.
	#
	# There are ungeneratable symbols, and the calculations here include a small
	#   amount of slack for that.
	#
	#
	unseenfreq, minfreq, unseenest = sum_over_singletons(sorted_grp, structure)
	totalfreq = sum_over_terminals(sorted_grp)

	if unseenfreq > 0:
		# First try a proper Good-Turing adjustment
		unseenprob = unseenfreq / totalfreq
		if unseenprob > 0.999:
			# Cap the unseen prob otherwise the next calculation won't work
			unseenprob = 0.999		
		adjtotalfreq = totalfreq / (1 - unseenprob)

		# We need the probability of any unseen terminal to be lower than the
		# probability of any seen terminal, because we always put unseen terminals
		# last in the terminal file and we assume that terminals are in decreasing
		# sorted order.
		# Check that the above calculation does not make this untrue.
		slack = 1.25  # Allow 25% slack for ungeneratable symbols
		minprobscaled = minfreq / adjtotalfreq
		if ((unseenprob / unseenest) * slack) > minprobscaled:
			# Adjust down unseenprob to make the above condition true
			# This is a rather tricky calculation...
			unseenprob = 1 / (1 + ((slack * totalfreq) / (unseenest * minfreq)))
			# All frequencies will be divided by totalfreq to compute a probability
			# Adjust this value based on the adjusted unseen probability, so that
			# the existing frequencies divided by this value, plus the unseen
			# probability mass, all equal 1.
			adjtotalfreq = totalfreq / (1 - unseenprob)
		return (unseenprob, adjtotalfreq)
	else:
		return (unseenfreq, totalfreq)

#
# MAIN SECTION
#
	
try:
	opts, args = getopt.gnu_getopt(sys.argv[1:], "?hvtgu",["help","verbose","usage", "noterminals", "nostructures", "unseenterminals"])
except getopt.GetoptError, err:
	print str(err)
	usage()
	sys.exit(2)

for o,a in opts:
	if o in ('-v','--verbose'):
		verbose = 1
	elif o in ('-h','-?','--help','--usage'):
		usage()
		sys.exit(0)
	elif o in ('-t', '--noterminals'):
		bTerminals = False
	elif o in ('-g', '--nostructures'):
		bStructures = False
	elif o in ('-u', '--unseenterminals'):
		bUnseenTerminals = True
	else:
		assert False, 'unhandled option'
if len(args)!=1:
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
discardcount = 0
for line in corpus:
	line = line.rstrip('\r\n')
	#print line
	line = line.encode('utf-8')  # Keep string in bytes representation

	fields = line.split("\t")
	inword = fields[0]
	infreq = float.fromhex("0x" + fields[1])  # hex float was stored without leading "0x"
	inID = fields[2]

	# Discard line if it exceeds MAXSTRUCTURELENGTH -- we will not bother
	# learning from it
	if len(inword) > MAXSTRUCTURELENGTH:
		discardcount += 1
		continue

	if bStructures:
		store_structure(get_structure(inword), infreq, inID)
	if bTerminals:
		t = extract_terminals(inword, infreq, inID)
# end for


# Process learned groups and frequencies
if bStructures:	
	process_structures()

if bTerminals:
	# terminal_groups is now frequency table of all terminal strings found in the training data
	# Sort terminals by frequency
	sorted_terminal_groups = sort_group(terminal_groups)	
	# Now group terminals by structure into a hash from structure to sorted list
	#   of terminals and frequencies
	structures_hash = group_sorted_keys(sorted_terminal_groups)
	terminal_structures = structures_hash.keys()
	for structure in terminal_structures:
		if bUnseenTerminals:
			unseenprob, totalfreq = good_turing(structures_hash[structure], structure)
		else:
			totalfreq = sum_over_terminals(structures_hash[structure])
		file = 'grammar/terminalRules/' + structure + '.txt'
		fd = open(file,'w')
		ctr = 0
		for freq, terminal_string in structures_hash[structure]:
			prob = freq / totalfreq
			hexprob = prob.hex()
			myids = (','.join(terminal_sets[terminal_string]))  # Join set elements with commas
			# line = "%s%s%0.30f\n" % (micro,'\x09', f4)
			fd.write(terminal_string + "\t" + hexprob + "\t" + myids + "\n")
			ctr += 1
		if bUnseenTerminals and unseenprob > 0:
			# Add a blank line, then a line with the unseen probability mass
			fd.write("\n")
			hexprob = unseenprob.hex()
			fd.write("<UNSEEN>\t" + hexprob + "\t" + structure + "\n")
		fd.close()
		#print "    %d terminal strings for structure %s" % (ctr,structure)

if verbose:
	# **********************************************
	sys.stderr.write('Summary of training corpus:\n')
	sys.stderr.write('\tStructures: ' + str(len(structure_groups)) + '\n')
	sys.stderr.write('\tUnique terminal strings: ' + str(len(terminal_groups)) + '\n')
	sys.stderr.write('\tDiscarded lines (too long):' + str(discardcount) + '\n')
	# **********************************************

