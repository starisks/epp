package epp;

public enum TokenType {
  // single-char
  LBRACKET, RBRACKET, LPAREN, RPAREN, COMMA, PLUS, MINUS, STAR, SLASH,

  // literals/identifiers
  NUMBER, STRING, IDENT,

  // structural
  NEWLINE, EOF,

  // keywords
  SET, TO, SAY, ASK, INTO,
  IF, THEN, ELSE, END,
  WHILE, DO,
  TRY, CATCH,
  FUNCTION, RETURN,
  AND, AT,
  IS, NOT, EQUAL, GREATER, LESS, THAN,
  TRUE, FALSE
}

