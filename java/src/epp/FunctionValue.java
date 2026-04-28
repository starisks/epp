package epp;

import epp.Ast.Stmt;

import java.util.List;

public final class FunctionValue {
  public final List<String> params;
  public final List<Stmt> body;
  public final Env closure;
  public final Span span;

  public FunctionValue(List<String> params, List<Stmt> body, Env closure, Span span) {
    this.params = params;
    this.body = body;
    this.closure = closure;
    this.span = span;
  }
}

