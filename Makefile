CXX = g++
CC = gcc
CXXFLAGS = -D_GNU_SOURCE -Wall -pthread
CFLAGS = -D_GNU_SOURCE -std=c99 -Wall -pthread
LDFLAGS = -lpthread

.PHONY: all
all: cpp-test c-test
	
cpp-test: cpp-test.o
	$(CXX) -o $@ $(CXXFLAGS) $(LDFLAGS) $^

c-test: c-test.o ftx_sync.o
	$(CC) -o $@ $(CFLAGS) $(LDFLAGS) $^

.cpp.o:
	$(CXX) $(CXXFLAGS) -c $<

.c.o:
	$(CC) $(CFLAGS) -c $<

cpp-test.cpp: c-test.cpp ftx_sync.h

c-test.cpp: ftx_sync.h

clean:
	rm -f *.o
	rm -f c-test
	rm -f cpp-test