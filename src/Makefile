CXX = g++
# Add -O0 to disable optimization for debugging, remove it for potentially faster code
# Add -fsanitize=address to CXXFLAGS and LDFLAGS to enable Address Sanitizer
CXXFLAGS = -std=c++17 -Wall -Wextra -g # -O0 -fsanitize=address
LDFLAGS = # -fsanitize=address

# Define targets
NOFORWARD_TARGET = noforward
FORWARD_TARGET = forward

# Source files for each target
NOFORWARD_SRCS = main.cpp processor.cpp
FORWARD_SRCS = forwarding.cpp

# Object files for each target
NOFORWARD_OBJS = $(NOFORWARD_SRCS:.cpp=.o)
FORWARD_OBJS = $(FORWARD_SRCS:.cpp=.o)

.PHONY: all clean

# Build both executables
all: $(NOFORWARD_TARGET) $(FORWARD_TARGET)

# Rule for noforward executable
$(NOFORWARD_TARGET): $(NOFORWARD_OBJS)
	$(CXX) $(LDFLAGS) $(NOFORWARD_OBJS) -o $(NOFORWARD_TARGET)

# Rule for forward executable
$(FORWARD_TARGET): $(FORWARD_OBJS)
	g++ -o forward forwarding.cpp

# Generic rule for compiling .cpp to .o
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Specific dependencies for noforward objects
main.o: main.cpp processor.hpp structures.hpp
processor.o: processor.cpp processor.hpp structures.hpp

# Specific dependency for forward object
forwarding.o: forwarding.cpp

clean:
	rm -f $(NOFORWARD_OBJS) $(FORWARD_OBJS) $(NOFORWARD_TARGET) $(FORWARD_TARGET)