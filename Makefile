all: bcache

nix.o: nix.cc
	$(CXX) -std=c++17 -c nix.cc -o nix.o

handle.o: handle.cc
	$(CXX) -std=c++17 -c handle.cc -o handle.o

config.o: config.cc
	$(CXX) -std=c++17 -c config.cc -o config.o

bcache: nix.o handle.o config.o bcache.cc
	$(CXX) -std=c++17 bcache.cc nix.o handle.o config.o -o bcache -lnixstore -lnixutil -lboost_filesystem -lboost_thread -lpthread -lboost_program_options

clean:
	rm -fv bcache *.o
