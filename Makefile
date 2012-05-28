CC=g++
CXX=g++
CXXFLAGS=-g -O2
LDFLAGS=-lrt -laio -lgsl -lgslcblas

rebench: rebench.o opts.o utils.o simulation.o io_engine.o io_engines.o workload.o stream_stat.o

rebench.o: opts.hpp utils.hpp simulation.hpp
opts.o: opts.hpp
utils.o: utils.hpp 
stream_stat.o: stream_stat.hpp utils.hpp
simulation.o: opts.hpp simulation.hpp io_engine.hpp
workload.o: workload.hpp
io_engine.o: io_engine.hpp workload.hpp io_engines.hpp stream_stat.hpp
io_engines.o: io_engine.hpp io_engines.hpp utils.hpp

clean:
	rm -f rebench.o rebench *~ *.o
