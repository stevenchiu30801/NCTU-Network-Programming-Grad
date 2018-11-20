CFLAG=-g -Wall
CC=gcc
CLIB=-pthread

all: np_simple np_single_proc np_multi_proc

np_simple: np_simple.o npshell_simple.o wrappers.o
	$(CC) $(CFLAG) -o np_simple np_simple.o npshell_simple.o wrappers.o

np_simple.o: np_simple.c np_simple.h header.h
	$(CC) $(CFLAG) -c -o np_simple.o np_simple.c

npshell_simple.o: npshell_simple.c npshell_simple.h header.h
	$(CC) $(CFLAG) -c -o npshell_simple.o npshell_simple.c

np_single_proc: np_single_proc.o npshell_single_proc.o wrappers.o
	$(CC) $(CFLAG) -o np_single_proc np_single_proc.o npshell_single_proc.o wrappers.o

np_single_proc.o: np_single_proc.c np_single_proc.h header.h
	$(CC) $(CFLAG) -c -o np_single_proc.o np_single_proc.c

npshell_single_proc.o: npshell_single_proc.c npshell_single_proc.h header.h
	$(CC) $(CFLAG) -c -o npshell_single_proc.o npshell_single_proc.c

np_multi_proc: np_multi_proc.o npshell_multi_proc.o wrappers.o
	$(CC) $(CFLAG) $(CLIB) -o np_multi_proc np_multi_proc.o npshell_multi_proc.o wrappers.o

np_multi_proc.o: np_multi_proc.c np_multi_proc.h header.h
	$(CC) $(CFLAG) -c -o np_multi_proc.o np_multi_proc.c

npshell_multi_proc.o: npshell_multi_proc.c npshell_multi_proc.h header.h
	$(CC) $(CFLAG) -c -o npshell_multi_proc.o npshell_multi_proc.c

wrappers.o: wrappers.c header.h
	$(CC) $(CFLAG) -c -o wrappers.o wrappers.c

clean:
	rm -f np_simple np_single_proc np_multi_proc *.o
