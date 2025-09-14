# Simple Makefile for building DISMv2 tests on Linux (development purposes)
# Note: The main project is Windows-specific and should be built with Visual Studio

CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -I. -DLINUX_BUILD
LDFLAGS = -lstdc++fs

# Source files for testing (excluding Windows-specific implementations)
TEST_SOURCES = unit_tests.cpp
MOCK_SOURCES = mock_cabhandler.cpp

# Object files
TEST_OBJECTS = $(TEST_SOURCES:.cpp=.o)
MOCK_OBJECTS = $(MOCK_SOURCES:.cpp=.o)

# Target executable
TEST_TARGET = unit_tests

.PHONY: all clean test

all: $(TEST_TARGET)

# Build the test executable with mock implementations
$(TEST_TARGET): $(TEST_OBJECTS) $(MOCK_OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# Compile test sources
unit_tests.o: unit_tests.cpp CabHandler_test.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Mock implementation for Linux testing
mock_cabhandler.o: mock_cabhandler.cpp CabHandler_test.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Run the tests
test: $(TEST_TARGET)
	./$(TEST_TARGET)

# Clean build artifacts
clean:
	rm -f *.o $(TEST_TARGET)

# Help target
help:
	@echo "Available targets:"
	@echo "  all     - Build test executable"
	@echo "  test    - Build and run tests"
	@echo "  clean   - Remove build artifacts"
	@echo "  help    - Show this help message"
	@echo ""
	@echo "Note: This Makefile is for Linux development testing only."
	@echo "The main Windows project should be built using Visual Studio."