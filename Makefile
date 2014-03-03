.PHONY: all binfolder clean

all: bin/blurbench

bin/blurbench: bin/board.o bin/hash.o src/main.cc
	g++ -Wall -Wextra -O2 -o bin/blurbench bin/board.o bin/hash.o src/main.cc

binfolder:
	mkdir -p bin

bin/hash.o: binfolder src/hash.h src/hash.cc
	g++ -Wall -Wextra -O2 -o bin/hash.o -c src/hash.cc

bin/board.o: binfolder src/hash.h src/board.h src/board.cc src/piece.h src/move.h src/rc4.h
	g++ -Wall -Wextra -O2 -o bin/board.o -c src/board.cc

clean:
	rm -rf bin
