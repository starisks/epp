package epp;

import epp.Ast.*;

import java.util.ArrayList;
import java.util.List;

public final class Parser {
  public static final class Result {
    public final List<Stmt> program;
    public final List<ParseError> errors;
    public Result(List<Stmt> program, List<ParseError> errors) {
      this.program = program;
      this.errors = errors;
    }
  }

  private final List<Token> tokens;
  private int i = 0;
  private final List<ParseError> errors = new ArrayList<>();

  private Parser(List<Token> tokens) {
    this.tokens = tokens;
  }

  public static Result parse(List<Token> tokens) {
    return new Parser(tokens).run();
  }

  private Result run() {
    List<Stmt> program = new ArrayList<>();
    while (!isAtEnd()) {
      if (match(TokenType.NEWLINE)) continue;
      Stmt st = statement();
      if (st != null) program.add(st);
      consumeNewlines();
    }
    return new Result(program, errors);
  }

  // ---- helpers ----
  private boolean isAtEnd() { return peek().type == TokenType.EOF; }
  private Token peek() { return tokens.get(i); }
  private Token previous() { return tokens.get(i - 1); }
  private boolean check(TokenType t) { return !isAtEnd() && peek().type == t; }
  private Token advance() { if (!isAtEnd()) i++; return previous(); }
  private boolean match(TokenType t) { if (check(t)) { advance(); return true; } return false; }
  private void consumeNewlines() { while (match(TokenType.NEWLINE)) {} }

  private Token expect(TokenType t, String msg) {
    if (check(t)) return advance();
    errors.add(new ParseError(msg, peek().span));
    return new Token(t, "", peek().span);
  }

  // ---- statements ----
  private Stmt statement() {
    if (match(TokenType.SET)) return setStmt(previous().span);
    if (match(TokenType.SAY)) return sayStmt(previous().span);
    if (match(TokenType.ASK)) return askStmt(previous().span);
    if (match(TokenType.IF)) return ifStmt(previous().span);
    if (match(TokenType.WHILE)) return whileStmt(previous().span);
    if (match(TokenType.TRY)) return tryCatchStmt(previous().span);
    if (match(TokenType.FUNCTION)) return functionStmt(previous().span);
    if (match(TokenType.RETURN)) return returnStmt(previous().span);

    Expr e = expression();
    if (e == null) return null;
    return new ExprStmt(e, e.span());
  }

  private Stmt setStmt(Span sp) {
    Token name = expect(TokenType.IDENT, "Expected variable name after 'set'");
    expect(TokenType.TO, "Expected 'to' after variable name");
    Expr val = expression();
    if (val == null) val = new NumberExpr(0, sp);
    return new SetStmt(name.lexeme, val, sp);
  }

  private Stmt sayStmt(Span sp) {
    Expr val = expression();
    if (val == null) val = new StringExpr("", sp);
    return new SayStmt(val, sp);
  }

  private Stmt askStmt(Span sp) {
    Expr prompt = expression();
    expect(TokenType.INTO, "Expected 'into' after ask prompt");
    Token name = expect(TokenType.IDENT, "Expected variable name after 'into'");
    if (prompt == null) prompt = new StringExpr("", sp);
    return new AskStmt(prompt, name.lexeme, sp);
  }

  private List<Stmt> blockUntil(TokenType... stop) {
    consumeNewlines();
    List<Stmt> out = new ArrayList<>();
    while (!isAtEnd()) {
      if (isStop(stop)) break;
      Stmt st = statement();
      if (st != null) out.add(st);
      consumeNewlines();
    }
    return out;
  }

  private boolean isStop(TokenType... stop) {
    for (TokenType t : stop) if (check(t)) return true;
    return false;
  }

  private Stmt ifStmt(Span spIf) {
    List<IfBranch> branches = new ArrayList<>();
    Expr cond = conditionExpr();
    expect(TokenType.THEN, "Expected 'then' after if condition");
    consumeNewlines();
    List<Stmt> body = blockUntil(TokenType.ELSE, TokenType.END);
    branches.add(new IfBranch(cond, body, spIf));

    List<Stmt> elseBody = new ArrayList<>();
    while (match(TokenType.ELSE)) {
      Span spElse = previous().span;
      if (match(TokenType.IF)) {
        Expr c = conditionExpr();
        expect(TokenType.THEN, "Expected 'then' after else if condition");
        consumeNewlines();
        List<Stmt> b = blockUntil(TokenType.ELSE, TokenType.END);
        branches.add(new IfBranch(c, b, spElse));
        continue;
      }
      consumeNewlines();
      elseBody = blockUntil(TokenType.END);
      break;
    }

    expect(TokenType.END, "Expected 'end' to close if statement");
    return new IfStmt(branches, elseBody, spIf);
  }

  private Stmt whileStmt(Span sp) {
    Expr cond = conditionExpr();
    expect(TokenType.DO, "Expected 'do' after while condition");
    consumeNewlines();
    List<Stmt> body = blockUntil(TokenType.END);
    expect(TokenType.END, "Expected 'end' to close while loop");
    if (cond == null) cond = new BoolExpr(false, sp);
    return new WhileStmt(cond, body, sp);
  }

  private Stmt tryCatchStmt(Span sp) {
    consumeNewlines();
    List<Stmt> tryBody = blockUntil(TokenType.CATCH, TokenType.END);
    List<Stmt> catchBody = new ArrayList<>();
    if (match(TokenType.CATCH)) {
      consumeNewlines();
      catchBody = blockUntil(TokenType.END);
    } else {
      errors.add(new ParseError("Expected 'catch' in try/catch", peek().span));
    }
    expect(TokenType.END, "Expected 'end' to close try/catch");
    return new TryCatchStmt(tryBody, catchBody, sp);
  }

  private Stmt functionStmt(Span sp) {
    Token name = expect(TokenType.IDENT, "Expected function name after 'function'");
    List<String> params = new ArrayList<>();
    if (check(TokenType.IDENT)) {
      params.add(advance().lexeme);
      while (match(TokenType.AND)) {
        Token p = expect(TokenType.IDENT, "Expected parameter name after 'and'");
        params.add(p.lexeme);
      }
    }
    consumeNewlines();
    List<Stmt> body = blockUntil(TokenType.END);
    expect(TokenType.END, "Expected 'end' to close function");
    return new FunctionStmt(name.lexeme, params, body, sp);
  }

  private Stmt returnStmt(Span sp) {
    if (check(TokenType.NEWLINE) || check(TokenType.END) || check(TokenType.ELSE) || check(TokenType.CATCH) || check(TokenType.EOF)) {
      return new ReturnStmt(null, sp);
    }
    Expr v = expression();
    return new ReturnStmt(v, sp);
  }

  // ---- expressions ----
  private static boolean startsExpr(TokenType t) {
    return switch (t) {
      case NUMBER, STRING, IDENT, LPAREN, LBRACKET, TRUE, FALSE, MINUS -> true;
      default -> false;
    };
  }

  private Expr conditionExpr() {
    Expr left = expression();
    if (left == null) return null;
    if (!match(TokenType.IS)) return left;

    boolean isNot = match(TokenType.NOT);

    if (match(TokenType.GREATER)) {
      match(TokenType.THAN);
      Expr right = expression();
      BinaryOp op = isNot ? BinaryOp.LTE : BinaryOp.GT;
      if (right == null) right = new NumberExpr(0, peek().span);
      return new BinaryExpr(op, left, right, left.span());
    }

    if (match(TokenType.LESS)) {
      match(TokenType.THAN);
      Expr right = expression();
      BinaryOp op = isNot ? BinaryOp.GTE : BinaryOp.LT;
      if (right == null) right = new NumberExpr(0, peek().span);
      return new BinaryExpr(op, left, right, left.span());
    }

    if (match(TokenType.EQUAL)) {
      match(TokenType.TO);
      Expr right = expression();
      BinaryOp op = isNot ? BinaryOp.NEQ : BinaryOp.EQ;
      if (right == null) right = new NumberExpr(0, peek().span);
      return new BinaryExpr(op, left, right, left.span());
    }

    if (isNot) {
      Expr right = expression();
      if (right == null) right = new NumberExpr(0, peek().span);
      return new BinaryExpr(BinaryOp.NEQ, left, right, left.span());
    }

    errors.add(new ParseError("Expected comparison after 'is'", peek().span));
    return left;
  }

  private Expr expression() { return addition(); }

  private Expr addition() {
    Expr e = multiplication();
    while (true) {
      if (match(TokenType.PLUS)) {
        Span sp = previous().span;
        Expr r = multiplication();
        if (r == null) r = new NumberExpr(0, sp);
        e = new BinaryExpr(BinaryOp.ADD, e, r, sp);
        continue;
      }
      if (match(TokenType.MINUS)) {
        Span sp = previous().span;
        Expr r = multiplication();
        if (r == null) r = new NumberExpr(0, sp);
        e = new BinaryExpr(BinaryOp.SUB, e, r, sp);
        continue;
      }
      break;
    }
    return e;
  }

  private Expr multiplication() {
    Expr e = unary();
    while (true) {
      if (match(TokenType.STAR)) {
        Span sp = previous().span;
        Expr r = unary();
        if (r == null) r = new NumberExpr(0, sp);
        e = new BinaryExpr(BinaryOp.MUL, e, r, sp);
        continue;
      }
      if (match(TokenType.SLASH)) {
        Span sp = previous().span;
        Expr r = unary();
        if (r == null) r = new NumberExpr(0, sp);
        e = new BinaryExpr(BinaryOp.DIV, e, r, sp);
        continue;
      }
      break;
    }
    return e;
  }

  private Expr unary() {
    if (match(TokenType.MINUS)) {
      Span sp = previous().span;
      Expr rhs = unary();
      if (rhs == null) rhs = new NumberExpr(0, sp);
      return new UnaryExpr(UnaryOp.NEG, rhs, sp);
    }
    return postfix();
  }

  private Expr postfix() {
    Expr e = primary();
    while (e != null && match(TokenType.AT)) {
      Span sp = previous().span;
      Expr idx = expression();
      if (idx == null) idx = new NumberExpr(0, sp);
      e = new IndexExpr(e, idx, sp);
    }
    return e;
  }

  private Expr primary() {
    if (match(TokenType.NUMBER)) {
      Token t = previous();
      double v = Double.parseDouble(t.lexeme);
      return new NumberExpr(v, t.span);
    }
    if (match(TokenType.STRING)) {
      Token t = previous();
      return new StringExpr(t.lexeme, t.span);
    }
    if (match(TokenType.TRUE)) return new BoolExpr(true, previous().span);
    if (match(TokenType.FALSE)) return new BoolExpr(false, previous().span);

    if (match(TokenType.LBRACKET)) {
      Span sp = previous().span;
      List<Expr> elems = new ArrayList<>();
      if (!check(TokenType.RBRACKET)) {
        do {
          Expr e = expression();
          if (e != null) elems.add(e);
        } while (match(TokenType.COMMA));
      }
      expect(TokenType.RBRACKET, "Expected ']' to close array literal");
      return new ArrayExpr(elems, sp);
    }

    if (match(TokenType.LPAREN)) {
      Span sp = previous().span;
      Expr inner = expression();
      expect(TokenType.RPAREN, "Expected ')' to close group");
      if (inner == null) inner = new NumberExpr(0, sp);
      return new GroupExpr(inner, sp);
    }

    if (match(TokenType.IDENT)) {
      Token id = previous();
      if (startsExpr(peek().type)) {
        List<Expr> args = new ArrayList<>();
        Expr first = expression();
        if (first != null) args.add(first);
        while (match(TokenType.AND)) {
          Expr a = expression();
          if (a != null) args.add(a);
        }
        return new CallExpr(id.lexeme, args, id.span);
      }
      return new VarExpr(id.lexeme, id.span);
    }

    errors.add(new ParseError("Expected expression", peek().span));
    advance();
    return null;
  }
}

