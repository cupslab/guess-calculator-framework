//////////////////////////////////////////////////////////////////////////////////////////////////////////
//   pcfg_manager - creates password guesses using a probablistic context free grammar
//
//   Copyright (C) Matt Weir <weir@cs.fsu.edu>
//
//   pcfg_manager.h
//
//


#ifndef _PCFG_MANAGER_H
#define _PCFG_MANAGER_H

#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <math.h>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <deque>
#include <list>
#include <queue>
#include <iterator>

using namespace std;

#define MAXWORDSIZE 17 //Maximum size of a word from the input dictionaries
#define MAXINPUTDIC 10  //Maximum number of user inputed dictionaries

///////////////////////////////////////////
//Used for initially parsing the dictionary words
typedef struct mainDicHolderStruct {
  string word;
  int category;
  double probability;
  short word_size;
}mainDicHolderType;


///////////////////////////////////////////
//Non-Terminal Container Struct
//Holds all the base information used for non-terminal to terminal replacements
typedef struct ntContainerStruct {
  list <string> word;           //the replacement value, can be a dictionary word, a
  double probability;    //the probability of this group
  bool replaceRule;            //if this is a replacement rule and not a final terminal
  ntContainerStruct *next;        //The next highest probable replacement for this type
  char type; //used for debugging, can delete later
  int typeSize;  //used for debugging, can delete later
}ntContainerType;

//////////////////////////////////////////
//PriorityQueue Replacement Type
//Basically a pointer
typedef struct pqReplacementStruct {
  deque <ntContainerStruct *> replacement;
  double probability;
  double base_probability;  //the probability of the base structure
  int pivotPoint;
}pqReplacementType;
#endif

