# Makefile for compiling a SystemC-based simulation project
# Places all object files (.o) in the ./build/ directory for better organization

# Compiler and paths
CXX = g++
SYSTEMC_HOME = /usr/local/systemc
YAML_CPP_HOME = $(HOME)/yaml-cpp-install

# Compilation flags (include headers and C++ standard)
CXXFLAGS = -I$(SYSTEMC_HOME)/include \
           -I$(YAML_CPP_HOME)/include \
           -std=c++17 -Wall -Wextra -Iinclude

# Linking flags (library paths and libraries)
LDFLAGS = -L$(SYSTEMC_HOME)/lib-linux64 \
          -L/usr/lib/x86_64-linux-gnu \
          -L$(YAML_CPP_HOME)/lib \
          -lsystemc -lyaml-cpp

# Source files
SRCS = $(wildcard src/*.cpp) main.cpp

# Object files (placed in ./build/)
OBJS = $(patsubst %.cpp,build/%.o,$(SRCS))

# Executable name
TARGET = $(project_name)_sim

# Create build directory if it doesn't exist
BUILD_DIR = build

# Default target
all: $(BUILD_DIR) $(TARGET)

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR) $(BUILD_DIR)/src

# Link the executable
$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $@ $(LDFLAGS)

# Compile .cpp to .o in build directory, depend on $(BUILD_DIR)
build/%.o: %.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean up build files
clean:
	rm -rf $(BUILD_DIR) $(TARGET)
	rm -rf output/*

.PHONY: all clean