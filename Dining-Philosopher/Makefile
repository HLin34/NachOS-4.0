CXX = g++
CXXFLAGS =  -pthread 
EXES     := main

.PHONY: all 
all: $(EXES)

.PHONY: clean
clean:
	rm -f $(EXES)

.PHONY: main 
main: main.cpp fork.cpp table.cpp philosopher.cpp 
	g++ $(CXXFLAGS) -o $@ $?