CXX := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -Iinclude

SRCS := src/main.cpp src/trace.cpp src/cache.cpp src/prefetch.cpp src/hierarchy.cpp
OBJS := $(SRCS:.cpp=.o)

BIN := cache_sim

all: $(BIN)

$(BIN): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(BIN)

.PHONY: all clean
