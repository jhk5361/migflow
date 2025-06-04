# Makefile for creating C++ library

# Compiler and flags
CXX = g++
CXXFLAGS +=	-I../ \
		   	-I../../linux-kernels/common \
			-I./thread-pool/include \
			-Wall -fPIC -std=c++17 \
			-lpthread \
			-O2

# Target names
STATIC_LIB = libkoo_mig.a
SHARED_LIB = libkoo_mig.so

TARGET_BIN = koo_mig

# Source files
SRC = koo_mig_inf.cc koo_mig.cc utils.cc profile.cc

# Object files
OBJ = $(SRC:.cc=.o)

# Default target
all: $(STATIC_LIB) $(SHARED_LIB) $(TARGET_BIN)

# Build static library
$(STATIC_LIB): $(OBJ)
	$(AR) rcs $@ $^

# Build shared library
$(SHARED_LIB): $(OBJ)
	$(CXX) -shared -o $@ $^

$(TARGET_BIN): $(OBJ) koo_mig_main.o
	$(CXX) -o $@ $^ -lnuma

# Compile source files to object files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean up
clean:
	rm -f $(TARGET_BIN) $(OBJ) $(STATIC_LIB) $(SHARED_LIB)

.PHONY: all clean
