CXX = g++

SYSTEMC_HOME = /usr/local/systemc
YAML_CPP_HOME = $(HOME)/yaml-cpp-install

CXXFLAGS = -std=c++11 -I. -I$(SYSTEMC_HOME)/include -L$(SYSTEMC_HOME)/lib -lsystemc

# Compilation flags (include headers)
CXXFLAGS = -I$(SYSTEMC_HOME)/include \
           -I$(YAML_CPP_HOME)/include \
           -std=c++17 -Wall -Wextra -Iinclude

# Linking flags (library paths + libraries)
LDFLAGS = -L$(SYSTEMC_HOME)/lib-linux64 \
          -L/usr/lib/x86_64-linux-gnu \
          -L$(YAMLCPP_HOME)/lib \
          -lsystemc -lyaml-cpp

SRCS = $(wildcard src/*.cpp) \
       main.cpp

OBJS = $(patsubst %.cpp,%.o,$(SRCS))

# Executable Name
TARGET = $(project_name)_sim

all: $(TARGET)

# Link the executable
$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $@ $(LDFLAGS)

# Compile .cpp to .o
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean up build files
clean:
	rm -f $(OBJS) $(TARGET)
	rm -rf output/*

.PHONY: all clean