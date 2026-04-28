package epp;

public final class LexError {
  public final String message;
  public final Span span;

  public LexError(String message, Span span) {
    this.message = message;
    this.span = span;
  }
}

