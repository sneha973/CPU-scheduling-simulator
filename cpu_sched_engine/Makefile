CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic
INC      := -Iinclude
LIB_SRC  := $(filter-out src/main.cpp,$(wildcard src/*.cpp))
LIB_OBJ  := $(LIB_SRC:src/%.cpp=build/%.o)

all: build/schedsim

build/%.o: src/%.cpp $(wildcard include/schedsim/*.hpp)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $(INC) -c $< -o $@

build/schedsim: $(LIB_OBJ) build/main.o
	$(CXX) $(CXXFLAGS) $^ -o $@

build/test_engine: tests/test_engine.cpp $(LIB_OBJ)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $(INC) $^ -o $@

test: build/test_engine
	./build/test_engine

clean:
	rm -rf build

.PHONY: all test clean
