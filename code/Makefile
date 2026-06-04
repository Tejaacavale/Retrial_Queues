CXX      = g++
CXXFLAGS = -std=c++11 -O2 -Wall -Wextra

.PHONY: all test python clean

all: compute

compute: compute.cpp simulate.h
	$(CXX) $(CXXFLAGS) -o compute compute.cpp

test: test_compute.cpp simulate.h
	$(CXX) $(CXXFLAGS) -o test_compute test_compute.cpp
	./test_compute

# Build the Python extension module (requires: pip install pybind11)
# Produces retrial_queue*.so next to this file.
python: bindings.cpp simulate.h setup.py
	python3 setup.py build_ext --inplace

clean:
	rm -f compute test_compute file1.txt file2.txt file3.txt
	rm -rf build *.so retrial_queue*.so retrial_queue*.pyd
