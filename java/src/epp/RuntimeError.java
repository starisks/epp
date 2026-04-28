package epp;

public final class RuntimeError extends RuntimeException {
  public final String message;
  public final Span span;

  public RuntimeError(String message, Span span) {
    super(message);
    this.message = message;
    this.span = span;
  }
}

