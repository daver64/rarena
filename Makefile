
CXX ?= c++
CXXFLAGS ?= -std=c++20 -Wall -Wextra -Wpedantic

.PHONY: all clean

all: arena_test

arena_test: arena_test.cpp include/rarena.h
	$(CXX) $(CXXFLAGS) -o $@ arena_test.cpp

clean:
	$(RM) arena_test
