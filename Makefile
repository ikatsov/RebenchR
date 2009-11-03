CC=g++
CXX=g++
CXXFLAGS=-g -O2
LDFLAGS=-lrt -lgsl -lgslcblas -lm

rebench: rebench.o opts.o utils.o operations.o simulation.o

rebench.o: opts.hpp utils.hpp operations.hpp simulation.hpp
opts.o: opts.hpp
utils.o: utils.hpp
operations.o: operations.hpp opts.hpp utils.hpp
simulation.o: opts.hpp simulation.hpp operations.hpp

clean:
	rm -f rebench.o rebench *~ *.o
