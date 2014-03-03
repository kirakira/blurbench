.PHONY: all binfolder clean

all: bin/blurbench

bin/blurbench: obj/board.o obj/hash.o src/main.cc
	mkdir -p bin
	g++ -Wall -Wextra -O2 -o bin/blurbench obj/board.o obj/hash.o src/main.cc

obj/hash.o: src/hash.h src/hash.cc
	mkdir -p obj
	g++ -Wall -Wextra -O2 -o obj/hash.o -c src/hash.cc

obj/board.o: src/hash.h src/board.h src/board.cc src/piece.h src/move.h src/rc4.h
	mkdir -p obj
	g++ -Wall -Wextra -O2 -o obj/board.o -c src/board.cc

clean:
	rm -rf obj
