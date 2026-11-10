CXX ?= g++
CPPFLAGS ?=
CXXFLAGS ?= -std=c++11 -Wall -Wextra -pthread
LDLIBS += -pthread
SOURCES = main.cpp fork.cpp table.cpp philosopher.cpp
HEADERS = config.hpp fork.hpp table.hpp philosopher.hpp

.PHONY: all clean
all: main

main: $(SOURCES) $(HEADERS)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(LDFLAGS) -o $@ $(SOURCES) $(LDLIBS)

clean:
	rm -f main
