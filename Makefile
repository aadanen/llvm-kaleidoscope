# variables
CC := clang++
CFLAGS := -Wall -Wpedantic -Iinclude
LDFLAGS := -rdynamic
LLVMCONFIG := `llvm-config --cxxflags --ldflags --system-libs --libs core orcjit native`
BINARY := build/kalos

# makefile functions are called with $(function <arguments>)
# wildcard allows a string with anything substituted for the *
SOURCE_FILES := $(wildcard src/*.cpp)
# patsubst has 3 arguments
# [the original pattern],[the new pattern],[data]
# the % sign is the part of the original pattern that gets copied into 
# the new pattern
OBJECT_FILES := $(patsubst src/%.cpp,build/%.o,$(SOURCE_FILES))
# these two functions in combination create two variables
# SOURCE_FILES automatically matches all .cpp files in the src folder
# OBJECT_FILES automatically maps each .cpp to a .o with the same name

# .PHONY targets aren't expected to create a file, you use them manually
# "all" is the special case thats called implicitly when you enter "make"
.PHONY: all clean

all: build $(OBJECT_FILES) $(BINARY)

# $^ is an automatic variable that expands to the whole dependency 
$(BINARY): $(OBJECT_FILES)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ $(LLVMCONFIG) -o $(BINARY)

build:
	mkdir -p build

# using % in the rules means it matches the pattern with % being variable
# $@ is the target
# $^ is the
build/%.o: src/%.cpp
	$(CC) $(CFLAGS) -c $^ -o $@

clean:
	rm -rf build
