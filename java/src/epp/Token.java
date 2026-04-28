package epp;

public final class Token {
  public final TokenType type;
  public final String lexeme;
  public final Span span;

  public Token(TokenType type, String lexeme, Span span) {
    this.type = type;
    this.lexeme = lexeme;
    this.span = span;
  }
}

