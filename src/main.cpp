#include <stdio.h>
#include <string>

/* LEXER */
enum Token {
  tok_eof = -1,
  tok_def = -2,
  tok_extern = -3,
  tok_identifier = -4,
  tok_number = -5,
};

static std::string IdentifierStr;
static double NumVal;

std::string tokenToString(int t) {
  switch (t) {
  case (tok_eof):
    return "TOK_EOF";
  case (tok_def):
    return "TOK_DEF";
  case (tok_extern):
    return "TOK_EXTERN";
  case (tok_identifier):
    return "TOK_IDENTIFIER " + IdentifierStr;
  case (tok_number):
    return "TOK_NUMBER " + std::to_string(NumVal);
  default:
    return "TOK_OPERATOR " + std::string(1, (char)t);
    // return "OPERATOR " + std::string((char)t);
  };
}

static int gettok() {
  // skip whitespace between tokens
  static int LastChar = ' ';
  while (isspace(LastChar)) {
    LastChar = getchar();
  }

  // identifier: [a-zA-Z][a-zA-Z0-9]
  if (isalpha(LastChar)) {
    IdentifierStr = LastChar;
    while (isalnum((LastChar = getchar())))
      IdentifierStr += LastChar;

    if (IdentifierStr == "def")
      return tok_def;
    if (IdentifierStr == "extern")
      return tok_extern;
    return tok_identifier;
  }

  // number: [0-9.]+
  if (isdigit(LastChar) || LastChar == '.') { // Number: [0-9.]+
    std::string NumStr;
    do {
      NumStr += LastChar;
      LastChar = getchar();
    } while (isdigit(LastChar) || LastChar == '.');

    NumVal = strtod(NumStr.c_str(), 0);
    return tok_number;
  }

  if (LastChar == '#') {
    // Comment until end of line.
    do
      LastChar = getchar();
    while (LastChar != EOF && LastChar != '\n' && LastChar != '\r');

    if (LastChar != EOF)
      return gettok();
  }

  // Check for end of file.  Don't eat the EOF.
  if (LastChar == EOF)
    return tok_eof;

  // Otherwise, just return the character as its ascii value.
  int ThisChar = LastChar;
  LastChar = getchar();
  return ThisChar;
}

int main(int argc, char **argv) {
  printf("Hello Kaleidoscope\n");

  int t;
  do {
    t = gettok();
    printf("%s\n", tokenToString(t).c_str());
  } while (t != tok_eof);
  return 0;
}
