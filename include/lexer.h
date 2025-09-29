#ifndef LEXER_H_
#define LEXER_H_

#include <string>
// The lexer returns tokens [0-255] if it is an unknown character, otherwise one
// of these for known things.
enum Token {
  tok_eof = -1,

  // commands
  tok_def = -2,
  tok_extern = -3,
  tok_global = -4,

  // primary
  tok_identifier = -5,
  tok_number = -6,

  // control
  tok_if = -7,
  tok_then = -8,
  tok_else = -9,
  tok_for = -10,
  tok_in = -11,

  // operators
  tok_binary = -12,
  tok_unary = -13,

  // var definition
  tok_var = -14
};

extern std::string IdentifierStr; // Filled in if tok_identifier
extern double NumVal;             // Filled in if tok_num
extern int LastChar;
extern bool USE_REPL;
extern std::string source;
int gettok();
#endif /* LEXER_H_ */
