CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -g # Use C++17, enable warnings, add debug info
LDFLAGS =

# Source files
SRCS = main.cpp processor.cpp
# Object files
OBJS = $(SRCS:.cpp=.o)

# Executable name
TARGET = noforward

# Default target
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(TARGET) $(LDFLAGS)

# Rule to compile .cpp files into .o files
%.o: %.cpp processor.hpp structures.hpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean target
clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean