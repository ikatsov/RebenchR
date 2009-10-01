CC=g++
CXX=g++
CXXFLAGS=-g -O2
LDFLAGS=-lrt -lgsl -lgslcblas -lm

rebench: rebench.o opts.o utils.o operations.o

rebench.o: opts.hpp utils.hpp operations.hpp
opts.o: opts.hpp
utils.o: utils.hpp
operations.o: operations.hpp opts.hpp utils.hpp

clean:
	rm -f rebench.o rebench *~ *.o
