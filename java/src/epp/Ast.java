package epp;

import java.util.ArrayList;
import java.util.List;

public final class Ast {
  private Ast() {}

  // ---- Expressions ----
  public interface Expr {
    Span span();
  }

  public record NumberExpr(double value, Span span) implements Expr {}
  public record StringExpr(String value, Span span) implements Expr {}
  public record BoolExpr(boolean value, Span span) implements Expr {}
  public record VarExpr(String name, Span span) implements Expr {}

  public record ArrayExpr(List<Expr> elements, Span span) implements Expr {}

  public enum BinaryOp { ADD, SUB, MUL, DIV, EQ, NEQ, GT, LT, GTE, LTE }
  public record BinaryExpr(BinaryOp op, Expr left, Expr right, Span span) implements Expr {}

  public enum UnaryOp { NEG }
  public record UnaryExpr(UnaryOp op, Expr expr, Span span) implements Expr {}

  public record CallExpr(String callee, List<Expr> args, Span span) implements Expr {}
  public record IndexExpr(Expr target, Expr index, Span span) implements Expr {}
  public record GroupExpr(Expr inner, Span span) implements Expr {}

  // ---- Statements ----
  public interface Stmt {
    Span span();
  }

  public record SetStmt(String name, Expr value, Span span) implements Stmt {}
  public record SayStmt(Expr value, Span span) implements Stmt {}
  public record AskStmt(Expr prompt, String intoName, Span span) implements Stmt {}

  public static final class IfBranch {
    public final Expr condition;
    public final List<Stmt> body;
    public final Span span;
    public IfBranch(Expr condition, List<Stmt> body, Span span) {
      this.condition = condition;
      this.body = body;
      this.span = span;
    }
  }

  public record IfStmt(List<IfBranch> branches, List<Stmt> elseBody, Span span) implements Stmt {}
  public record WhileStmt(Expr condition, List<Stmt> body, Span span) implements Stmt {}
  public record TryCatchStmt(List<Stmt> tryBody, List<Stmt> catchBody, Span span) implements Stmt {}
  public record FunctionStmt(String name, List<String> params, List<Stmt> body, Span span) implements Stmt {}
  public record ReturnStmt(Expr valueOrNull, Span span) implements Stmt {}
  public record ExprStmt(Expr expr, Span span) implements Stmt {}

  public static List<Stmt> newBlock() { return new ArrayList<>(); }
}

