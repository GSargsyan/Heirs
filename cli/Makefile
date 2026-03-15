CXX = g++
CXXFLAGS = -std=c++17 -O3

all: match

match: match.cpp ../src/board.cpp ../src/engine_v1.cpp
	$(CXX) $(CXXFLAGS) -o match match.cpp ../src/board.cpp ../src/engine_v1.cpp

clean:
	rm -f match
