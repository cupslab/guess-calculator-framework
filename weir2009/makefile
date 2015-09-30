CC=g++
XFLAG =-Wall -ansi -pedantic
CFLAG =
main: pcfg_manager checkPasswords


pcfg_manager: pcfg_manager.o 
	$(CC) $(CFLAG) pcfg_manager.o -o pcfg_manager
	
checkPasswords: checkPasswords.o
	$(CC) $(CFLAG) checkPasswords.o -o checkPasswords

pcfg_manager.o: pcfg_manager.cpp
	$(CC) $(CFLAG) -c pcfg_manager.cpp

checkPasswords.o: checkPasswords.c
	$(CC) $(CFLAG) -c checkPasswords.c

clean:
	rm -f pcfg_manager 
	rm -f checkPasswords
	rm -f *.o
