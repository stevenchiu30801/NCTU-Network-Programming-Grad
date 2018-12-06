CC=g++
CFLAG=-std=c++11 -g -Wall
CLIB=-lboost_system -lboost_thread -lpthread

all: http_server console.cgi

http_server: http_server.o
	$(CC) $(CFLAG) -o http_server http_server.o $(CLIB)

http_server.o: http_server.cpp
	$(CC) $(CFLAG) -c -o http_server.o http_server.cpp

console.cgi: console.o
	$(CC) $(CFLAG) -o console.cgi console.o $(CLIB)

console.o: console.cpp
	$(CC) $(CFLAG) -c -o console.o console.cpp

clean:
	rm -r http_server console.cgi *.o
