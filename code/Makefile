CXX      = g++
CXXFLAGS = -std=c++11 -O2 -Wall -Wextra

.PHONY: all test clean

all: compute

compute: compute.cpp simulate.h
	$(CXX) $(CXXFLAGS) -o compute compute.cpp

test: test_compute.cpp simulate.h
	$(CXX) $(CXXFLAGS) -o test_compute test_compute.cpp
	./test_compute

clean:
	rm -f compute test_compute file1.txt file2.txt file3.txt
