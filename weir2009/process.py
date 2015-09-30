#!/usr/bin/python 
# -*- coding: UTF-8 -*-
import sys, os, string, getopt, codecs
import locale
reload(sys)
sys.setdefaultencoding("utf_8")
locale.setlocale(locale.LC_ALL,"")

verbose=0

macro_groups = {}
micro_groups = {}
digit_groups = {}
special_groups = {}

def usage():
	print 'Usage: process.py [OPTIONS]* <training corpus>'
	print ' -v, --verbose\tPrint out summary of training corpus'
	print
	print 'Help options:'
	print ' -?, -h, --help\tShow this help message'
	print ' --usage\tDisplay brief usage message'
	print

def isAlphaInternational(w):
	if w.isalpha():
		return 1  
	return 0


def micro_structure(n):
	ret = ''
	char = ''
	for x in range(0,len(n)):
		if isAlphaInternational(n[x]):
			ret += "L"
		elif n[x].isdigit():
			ret += "D"
		else:
			ret += "S"
	return ret

def macro_structure(y):
	ret = u''
	char = u''
	n = micro_structure(y)
	for x in range(0,len(n)):
		if not (n[x].isspace()):
			if n[x] != char:
				ret+=n[x]
				char = n[x]
	return ret

def get_index(arr,val):
	pos = -1
	for x in range(0,len(arr)):
		try:	
			pos = arr[x].index(val)
			return x
		except:
			continue
	return pos

def add_macro_group(m):
	try:
		macro_groups[m]+=1
	except:
		macro_groups[m]=1

def add_micro_group(m):
	try:
		micro_groups[m]+=1
	except:
		micro_groups[m]=1

def sort_group(g):
	g = [(v,k) for k,v in g.items()]
	g.sort()
	g.reverse()
	return g

def extract_digits(line):
	d = []
	val = ''
	for x in range(0,len(line)):
		if(line[x].isdigit()):
			val += line[x]
		else:
			if(val!=''):
				d.append(val)
				val=''
	if(val != ''):
		d.append(val)
	#d contains all the digits found in the line
	for y in range(0,len(d)):
		try:
			digit_groups[d[y]]+=1
		except:
			digit_groups[d[y]]=1
		if len(str(y))==11:
			print "--->",y
	return d

def extract_special(line):
	s = []
	val = ''
	for x in range(0,len(line)):
		if((not(isAlphaInternational(line[x])))and(not(line[x].isdigit()))):
			val += line[x]
		else:
			if(val!=''):
				s.append(val)
				val=''
	if(val!=''):
		s.append(val)
	#s contains all special characters in the line	
	for y in range(0,len(s)):
		try:
			special_groups[s[y]]+=1
		except:
			special_groups[s[y]]=1
	return s

def calculate_total(grp):
	total = 0
	for count,value in grp:
		total += count
	return total

def simplify(n):
	ret = ''
	char = ''
	for x in range(0,len(n)):
		if not (n[x].isspace()):
			if n[x] != char:
				ret+=n[x]
				char = n[x]
	return ret

def process_groups():
	m = sort_group(macro_groups)
	macro_total = calculate_total(m)
	fd = open('grammar/structures.txt','w')
	for count, micro in micro_groups:
		f1 = macro_groups[simplify(micro)]
		f2 = float(f1)/macro_total
		f3 = float(count)/f1
		f4 = f2*f3
		line = "%s%s%0.30f\n" % (micro,'\x09', f4)
		fd.write(line)
	fd.close()

def calculate_terminals(grp,l):
	total = 0
	for k in grp.keys():
		tmp = str(k)
		if len(tmp)==l:
			#print k,tmp,grp[k]
			total+=grp[k]
			
	return total

def group_keys(grp):
	keys = []
	for k in grp.keys():
		tmp = len(str(k))
		try:
			keys.index(tmp)
		except:
			keys.append(tmp)
	return keys

#
# MAIN SECTION
#
	
try:
	opts, args = getopt.gnu_getopt(sys.argv[1:], "?hv",["help","verbose","usage"])
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
	else:
		assert False, 'unhandled option'
if len(args)!=1:
	usage()
	sys.exit(2)

corpus = open(args[0],'r')
line=u''
for line in corpus:
	line = line.rstrip()
	#print line
	line =line.decode('utf-8')
	m1 = macro_structure(line)
	m2 = micro_structure(line)
	add_macro_group(m1)
	add_micro_group(m2)
	d = extract_digits(line)
	s = extract_special(line)

micro_groups = sort_group(micro_groups)
tmp_digit_groups = sort_group(digit_groups)
tmp_special_groups = sort_group(special_groups)

process_groups()

tmp = group_keys(digit_groups)
for x in tmp:
	t = calculate_terminals(digit_groups,x)
	file = 'digits/'+str(x)+'.txt'
	fd = open(file,'w')
	for v,k in tmp_digit_groups:
		if len(str(k))==x:
			tmp_t = v
			#print x,k,tmp_t,t
			line = "%s%s%0.30f\n" % (str(k),'\x09',float(tmp_t)/t)
			fd.write(line)
	fd.close()

tmp = group_keys(special_groups)
for x in tmp:
	t = calculate_terminals(special_groups,x)
	file = 'special/'+str(x)+'.txt'
	fd = open(file,'w')
	for v,k in tmp_special_groups:
		if len(str(k))==x:
			tmp_t = v
			#print x,k,tmp_t,t
			line = "%s%s%0.30f\n" % (str(k),'\x09',float(tmp_t)/t)
			fd.write(line)
	fd.close()

#
# At this point we have generated the micro patterns
# without the terminal substitutions.  We have populated
# the digits/ and special/ directories with the terminal
# values of similar lengths stored in the appropriate file
#
if verbose:
	print '**********************************************'
	print '* Summary of training corpus:'
	print '*\tMacro structures:',len(macro_groups)
	print '*\tMicro structures:',len(micro_groups)
	print '*\tUnique digits:',len(digit_groups)
	print '*\tUnique special characters:',len(special_groups)
	print '**********************************************'

