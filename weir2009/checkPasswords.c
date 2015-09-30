/////////////////////////////////////////////////////////////////////
//Note, this code was originally written by Matt Weir, at Florida State University
//Contact Info weir -at- cs.fsu.edu
//This code is licensed GPLv2
//If you make any changes I would love to hear about them so I don't have to impliment them myself ;)
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <math.h>


#define MAXWORDSIZE 50 /*Maximum size of a password to check*/


/////////////////////////////////////////////
//Used to store all the passwords to check
//It's a little counterintuitive, but think of the levels as letter position
//let's say you have the words apple and apply stored
//For the first level you would only have an 'a' with nextChar=NULL
//nextLevel would point to 'p', and after following it, nextChar would still =NULL
//The same would keep happening until you hit 'l'
//next level would point to 'e', and once you follow it, nextChar='y'.  
//Therefore nextlevel goes along the word, while nextChar shows all the possible variation up to that level.
//If you had a third word, "bannana" it would diverge starting at the very first level
//If I had more time, I might allow words to reconnect to save space, but right now they are completly seperate
//so right now "cat" and "hat" will not share any of the same pointers, even though their last two letters are the same
typedef struct wordStruct {
  char letter;              //The letter to use
  int isFound;              //if the password has been found already
  int isTerm;               //if the node is the final value
  wordStruct* nextLevel;    //Goes to the next letter in the word
  wordStruct* nextChar;     //Goes to the next possible character for the current level
}wordType;



wordType *passwordList;     //Holds all the passwords to check
long totalFound;             //The total number of passwords that have been found
unsigned long totalGuesses;          //The total number of guesses made so far
long totalPasswords;         //The totalt number of passwords that we are guessing against
unsigned long limitGuesses;          //Limit the amount of guesses allowed, if 0, no limit
long stepSize;              //The x-axis stepsize to use in the graph friendly output
char userOutputFile[100];   //the file to save output to, otherwise stdout

void sig_alrm(int signo)
{
  printf("%i\t%i\n",totalGuesses, totalFound);
  exit(0);
}

int initilize();				 //Main fuction used to initilize variables
int readDic(char *userDic);			 //Used to prepare the dictionary
int getNextWord(FILE *inputFile,char *nextWord); //gets the next word for the dictionary
int getInput(int outputType,int outputLoc);      //reads in the next guess
int checkGuess(char *guess, long size);		 //checks to see if that guess is correct

int main(int argc, char *argv[]) {
  struct sigaction action;
  int badCommandLine;     //True if improper values are passed via the command line
  int outputType;         //0=found passwords, 1=graph friendly verion
  char userPassword[50];  //the user supplied password files to check
  int numUserDic;         //the number of password files to check, currently only supports 1.
  int i;
  int j;
  float tempValue;
  int isOutputFile;       //used to determine if it prints to a file=1, or stdout=0
//---------Parse the command line------------------------//
  badCommandLine =0;
  outputType = 0;
  numUserDic=0;
  limitGuesses=0;
  isOutputFile=0;
  if (argc == 1) {
    badCommandLine=1;
  }
  for (i=1;i<argc;i++) {
    if (argv[i][0]!='-') {
      if (i!=argc-1) {   //the password file to check should be the final input
        badCommandLine=1;
        break;
      }
      strncpy(userPassword,argv[i],49);
      userPassword[49]='\0';  //safty check
      numUserDic++;
    }
    else if ((argv[i][1]!='l')&&(argv[i][1]!='g')&&(argv[i][1]!='f')&&(argv[i][1]!='q')) { //eventually may want to support other output display types
      badCommandLine=1;
      break;
    }
    else if (argv[i][1]=='g') {  //if the user wants to output data in a graph friendly format
      if (outputType!=0) {  //if a different flag was already specified
        badCommandLine=1;
        break;
      }
      else {
        outputType=1;
        i++;
        if (i==argc) {
          badCommandLine=1;
        }
        else {
          stepSize=atoi(argv[i]);
          if (stepSize<=0) {
            badCommandLine=1;
          }
        }

      }
    }
    else if (argv[i][1]=='l') {  //if there is a limit
      i++;
      if (i<argc) {
        limitGuesses=atoi(argv[i]);
      }
      else {
        badCommandLine=1;
      }
      if (limitGuesses==0) {
        badCommandLine=1;
      }
    }
    else if (argv[i][1]=='f') { //the user wants to save to an outputfile
      i++;
      if (i<argc) {
        isOutputFile=1;
        strncpy(userOutputFile,argv[i],99);
      }
    }
    else if (argv[i][1]=='q') {
      if (outputType!=0) {
        badCommandLine=1;
        break;
      }
      else {
        outputType=2;
      }
    } 
  }
  if (numUserDic<1) {   //No dictionaries were entered
    badCommandLine=1;
  }
  if (badCommandLine) {
    printf("Usage: ./checkPassword <passwordfile>\n");
    printf("Aka    ./checkPassword passwords.txt                              will print out the passwords found\n");
    printf("Or     ./checkPassword -g <x-axis step size> passwords.txt        to print a graph friendly version\n");
    printf("Or     ./checkPassword -l <number of guesses> passwords.txt       to limit the number of guesses allowed\n");
    printf("Or     ./checkPassword -f <file to save output to> passwords.txt  saves output to a file, prints the status to stdout\n\n");
    exit(0);
  }

//-----------------End Parsing the Command Line------------//
//-----------------Signal Initilization--------------
  action.sa_handler = sig_alrm;
  sigemptyset(&action.sa_mask);
  action.sa_flags = 0;
  sigaction(SIGINT, &action, 0);
  sigaction(SIGKILL, &action, 0);
  sigaction(SIGTERM, &action, 0);
  sigaction(SIGALRM, &action, 0);

//-----------------End Signal Initilization----------
  passwordList=NULL;
  totalPasswords=0;
  totalFound=0;
  totalGuesses=0;
  if (readDic(userPassword)!=0) {  //initilize the dictionary
    printf("Could not open the dictionary file %s\n",userPassword);
    return -1;
  }
  getInput(outputType,isOutputFile);  //Does the actual checking to see if there are matches
  if (outputType!=2) {
      printf("%u\t%i\n",totalGuesses, totalFound);
  }
  else {
    tempValue= (totalFound/(totalPasswords*1.0))*100;
    printf("\n------------------------------------------\n");
    printf("Total Passwords: %i\n",totalPasswords);
    printf("Total number of guesses made: %u\n",totalGuesses);
    printf("Total number of passwords cracked: %i\n", totalFound);
    printf("Percentage of passwords cracked: %f%%\n",tempValue);
    printf("------------------------------------------\n");
  }
  return 0;
}


//////////////////////////////////////////////////////
//Function: readDic
//What it does: Parses a dictinary into the tree format for easy checking
//Returns 0   if everything goes ok
//        -1  if the file does not exist
int readDic(char *userDic) {
  FILE *inputFile;
  char nextWord[MAXWORDSIZE+1];
  int done;
  char curChar;
  int i;
  wordType *curPos;
  wordType *tempWordType;
  int size;
  inputFile=fopen(userDic,"r");
  if (inputFile==NULL) {
    return -1;
  }  
  done=getNextWord(inputFile,nextWord);
  size = strlen(nextWord);
  while (done!=1) {
    if (done!=-1) {  //if the dictionary word is valid
    //  printf("%s\n",nextWord);
      curPos=passwordList;
      for (i=0;i<size;i++) {
        //------------------------First search for the letter at the current level-------------
        curChar=nextWord[i];
        if (curPos!=NULL) {
          while ((curPos->letter!=curChar)&&(curPos->nextChar!=NULL)) {
            curPos=curPos->nextChar;
          }
        }
        if ((curPos==NULL)||(curPos->letter!=curChar)) { //Need to insert a new character in the same level
          tempWordType= new wordType;
          tempWordType->letter=curChar;
          tempWordType->nextLevel=NULL;
          tempWordType->nextChar=NULL;
          tempWordType->isFound=0;
          tempWordType->isTerm=0;
          if (curPos==NULL) {
            passwordList=tempWordType;
            curPos=passwordList;
          }
          else {
            curPos->nextChar=tempWordType;
            curPos=curPos->nextChar;
          }
        }
        //--------------Check to see if the next level is null, if so insert the next level
        if ((i<size-1)&&(curPos->nextLevel==NULL)) {  //if next level is empty and there is another letter to insert
          tempWordType=new wordType;
          tempWordType->letter=nextWord[i+1];
          tempWordType->nextLevel=NULL;
          tempWordType->nextChar=NULL;
          curPos->nextLevel=tempWordType;
          tempWordType->isTerm=0;
          tempWordType->isFound=0;
        }
        if (i<size-1) {
          curPos=curPos->nextLevel;
        }
      }
      //increment the word count
      totalPasswords++;
      curPos->isTerm++;
      
    }
    done=getNextWord(inputFile,nextWord);
    size=strlen(nextWord);
  }
  fclose(inputFile);
  return 0;
}


//////////////////////////////////////////////////////////
//gets the next word from the inputfile
//processes all characters
//does NOT convert characters to lower case
//returns 0 if everything works ok
//returns 1 if end of file
//returns -1 if the word is not valid
int getNextWord(FILE *inputFile,char *nextWord) {
  char tempChar;
  long curCount=0;

  tempChar=fgetc(inputFile);
  while(!feof(inputFile)&&(tempChar!='\n')&&(tempChar!='\r')) {
    if (curCount < (MAXWORDSIZE)) {
      nextWord[curCount]=tempChar;
      curCount++;
    }
    else {
      return -1;
    }
    tempChar=fgetc(inputFile);
  }
  nextWord[curCount]='\0';  
  if (feof(inputFile)){
    return 1;
  }
  else {
    return 0;
  }
} 


int getInput(int outputType,int outputLoc) {
  char userInput[MAXWORDSIZE*3+1];
  long size;
  long foundinRound;  //the number of passwords matched by the current guess
  FILE *outputFile;
  unsigned long lastOutput;  //used so we don't have to do division
  long isAlive;      //used to print status to the screen

  lastOutput=0;
  isAlive=0;
  if (outputLoc==1) {
    outputFile=fopen(userOutputFile,"w");
    if (outputFile==NULL) {
      printf("Can not open output file\n");
      return -1;
    }
    printf("\nStatus:");
  }
  while (fgets(userInput,MAXWORDSIZE*3,stdin)!=NULL) {  //Why a multiple of 3, why not?
    size = strlen(userInput);
    if ((userInput[size-1]=='\n')||(userInput[size-1]=='\r')) { //get rid of the trailing newline
      size--;
      userInput[size]='\0';
    }
    foundinRound=checkGuess(userInput,size);

    //---Note, yes I realize it would be easier to create the output string, then pass it to the file/stdout at the end--//
    //---But I realized it after I wrote and tested most of this, that's the problem with feature creap------------------//
    if (foundinRound) {
      totalFound=totalFound+foundinRound;
      if (outputType==0) {
        if (outputLoc==0) {
          printf("Total:%i\tNumber of Guesses:%u\t\tFound:'%s' number of instances:%i\n",totalFound, totalGuesses,userInput,foundinRound);
        }
        else {
          fprintf(outputFile,"Total:%i\tNumber of Guesses:%u\t\tFound:'%s'\n",totalFound, totalGuesses,userInput);
          printf("!");
        }
      }
      else if (outputType==1) {   //print the graph friendly output
        if (outputLoc==0) {
          if (totalFound%100==0) {
            printf("%u\t%i\n",totalGuesses, totalFound);
          }
        }
        else {
          fprintf(outputFile, "%u\t%i\n",totalGuesses, totalFound);
        }
      }
    }
    totalGuesses++;
/*    if (outputType==1) {          //print the graph friendly output
      if ((totalGuesses-stepSize)>=lastOutput) {
        isAlive++;
        if (isAlive==10) {
          printf("\n");
          isAlive=0;
        }
        lastOutput=totalGuesses;
        if (outputLoc==0) {
          printf("%u\t%i\n",totalGuesses, totalFound);
        }
        else {
          //fprintf(outputFile,"%u\t%i\n",totalGuesses, totalFound);
          printf(".");
        }
      }
    }
*/
    if ((limitGuesses!=0)&&(totalGuesses==limitGuesses)) {
      return 0;
    }
  }

  return 0;
}

///////////////////////////////////////////
//Returns the number of passwords matched by the current guess
//Note, it can return more than one since the password might
//have occured more than once in the input file
//aka 'password1'
int checkGuess(char *guess,long size) {
  wordType *curPos; 
  long i;

  curPos=passwordList;
  for (i=0;i<size;i++) {
    while ((curPos!=NULL)&&(curPos->letter!=guess[i])) {
      curPos=curPos->nextChar;
    }
    if (curPos==NULL) {
      return 0;
    }
    else if (i<(size-1)) { //Not the last character in the guess
      curPos=curPos->nextLevel;
    }
    else {  //it is the last letter
      if (curPos->isTerm) { //it is a password
        if (!curPos->isFound) { //it hasn't been found yet  
          curPos->isFound=1;
          return curPos->isTerm;
        }
      }
    }
  }
  return 0;
}
