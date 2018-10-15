CFLAG= -g -Wall
CC=gcc

all: npshell
	
npshell: npshell.o wrappers.o
	$(CC) $(CFLAG) -o npshell npshell.o wrappers.o

npshell.o: npshell.c npshell.h header.h
	$(CC) $(CFLAG) -c -o npshell.o npshell.c

wrappers.o: wrappers.c header.h
	$(CC) $(CFLAG) -c -o wrappers.o wrappers.c

clean:
	rm -f npshell *.o
