#include <lexer.h>
#include <string>

std::string IdentifierStr = "";
double NumVal = 0.0;
int LastChar = ' ';
std::string source = "";
int idx = 0;
bool USE_REPL = true;

int getNextCharacter() {
  if (USE_REPL) {
    return getchar();
  } else {
    if (idx < source.size()) {
      return source[idx++];
    } else {
      return EOF;
    }
  }
}
//===----------------------------------------------------------------------===//
// Lexer
//===----------------------------------------------------------------------===//

/// gettok - Return the next token from standard input.
int gettok() {

  // Skip any whitespace.
  while (isspace(LastChar))
    LastChar = getNextCharacter();

  if (isalpha(LastChar)) { // identifier: [a-zA-Z][a-zA-Z0-9]*
    IdentifierStr = LastChar;
    while (isalnum((LastChar = getNextCharacter())))
      IdentifierStr += LastChar;

    if (IdentifierStr == "def")
      return tok_def;
    if (IdentifierStr == "extern")
      return tok_extern;
    if (IdentifierStr == "global")
      return tok_global;
    if (IdentifierStr == "if")
      return tok_if;
    if (IdentifierStr == "then")
      return tok_then;
    if (IdentifierStr == "else")
      return tok_else;
    if (IdentifierStr == "for")
      return tok_for;
    if (IdentifierStr == "in")
      return tok_in;
    if (IdentifierStr == "binary")
      return tok_binary;
    if (IdentifierStr == "unary")
      return tok_unary;
    if (IdentifierStr == "var")
      return tok_var;
    return tok_identifier;
  }

  if (isdigit(LastChar) || LastChar == '.') { // Number: [0-9.]+
    std::string NumStr;
    do {
      NumStr += LastChar;
      LastChar = getNextCharacter();
    } while (isdigit(LastChar) || LastChar == '.');

    NumVal = strtod(NumStr.c_str(), nullptr);
    return tok_number;
  }

  if (LastChar == '#') {
    // Comment until end of line.
    do
      LastChar = getNextCharacter();
    while (LastChar != EOF && LastChar != '\n' && LastChar != '\r');

    if (LastChar != EOF)
      return gettok();
  }

  // Check for end of file.  Don't eat the EOF.
  if (LastChar == EOF)
    return tok_eof;

  // Otherwise, just return the character as its ascii value.
  int ThisChar = LastChar;
  LastChar = getNextCharacter();
  return ThisChar;
}
