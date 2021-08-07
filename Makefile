all: bcache

nix.o: nix.cc
	g++ -std=c++17 -c nix.cc -o nix.o

bcache: nix.o bcache.cc
	g++ -std=c++17 bcache.cc nix.o -o bcache -lnixstore -lnixutil -lboost_filesystem -lboost_thread -lpthread

clean:
	rm -fv bcache nix.o
