CC=g++
CFLAG=-std=c++11 -g -Wall
CLIB=-lboost_system -lboost_thread -lpthread

all: socks_server hw4.cgi

socks_server: socks_server.o
	$(CC) $(CFLAG) -o socks_server socks_server.o $(CLIB)

socks_server.o: socks_server.cpp
	$(CC) $(CFLAG) -c -o socks_server.o socks_server.cpp

hw4.cgi: console.o
	$(CC) $(CFLAG) -o hw4.cgi console.o $(CLIB)

console.o: console.cpp
	$(CC) $(CFLAG) -c -o console.o console.cpp 

clean:
	rm -r socks_server hw4.cgi *.o
