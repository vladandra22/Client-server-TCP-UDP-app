CC=g++ -std=c++11 -g -Wall -Wextra

all: build

build: server subscriber

server.o: server.cpp
	$(CC) $^ -c

subscriber.o: subscriber.cpp
	$(CC) $^ -c

clean:
	rm *.o server subscriber