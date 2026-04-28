package epp;

public final class ParseError {
  public final String message;
  public final Span span;

  public ParseError(String message, Span span) {
    this.message = message;
    this.span = span;
  }
}

