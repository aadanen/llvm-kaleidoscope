# Kalos
An interpretation of the LLVM kaleidoscope tutorial currently being expanded with addition features.

## Example
```
# compile kalos
make

# compile a kaleidoscope file to an object file
./build/kalos kl/mandel.kl -o mandel.o

# use a c++ compiler to create an executable
clang++ kl/mandel.o kl/main.cpp
./a.out
```

## Additional Features
- Improved the legibility of the codebase and reduced build times by refactoring the project into multiple files
- Added command line options to let users choose how they want to interact with the language
  - Input can be provided through a file or a REPL system
  - Output can take advantage of a JIT compiler or produce an object file
- Object files can be linked with a .c or .cpp file to produce an executable

## In Progress
- Global variable declaration and assignment

## Wishlist
- Ability to create executables without relying on a another compiler
- Memory management with functions like malloc and free
