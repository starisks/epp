package epp;

import epp.Ast.*;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.PrintStream;
import java.util.ArrayList;
import java.util.List;

public final class Interpreter {
  public static final class ExecResult {
    public final boolean ok;
    public final List<RuntimeError> errors;
    public ExecResult(boolean ok, List<RuntimeError> errors) {
      this.ok = ok;
      this.errors = errors;
    }
  }

  private final BufferedReader in;
  private final PrintStream out;
  private final Env globals;

  public Interpreter() {
    this(new BufferedReader(new InputStreamReader(System.in)), System.out);
  }

  public Interpreter(BufferedReader in, PrintStream out) {
    this.in = in;
    this.out = out;
    this.globals = new Env(null);
  }

  public ExecResult exec(List<Stmt> program) {
    List<RuntimeError> errors = new ArrayList<>();
    try {
      for (Stmt st : program) execStmt(st, globals);
      return new ExecResult(true, errors);
    } catch (RuntimeError e) {
      errors.add(e);
      return new ExecResult(false, errors);
    }
  }

  private static final class ReturnSignal extends RuntimeException {
    final Value value;
    ReturnSignal(Value value) { this.value = value; }
  }

  private RuntimeError err(String msg, Span sp) { return new RuntimeError(msg, sp); }

  private Value eval(Expr e, Env env) {
    if (e instanceof NumberExpr ne) return Value.num(ne.value());
    if (e instanceof StringExpr se) return Value.str(se.value());
    if (e instanceof BoolExpr be) return Value.bol(be.value());
    if (e instanceof VarExpr ve) {
      Holder<Value> h = new Holder<>();
      if (!env.get(ve.name(), h)) throw err("Undefined variable: " + ve.name(), ve.span());
      return h.value;
    }
    if (e instanceof GroupExpr ge) return eval(ge.inner(), env);
    if (e instanceof ArrayExpr ae) {
      List<Value> arr = new ArrayList<>();
      for (Expr el : ae.elements()) arr.add(eval(el, env));
      return Value.arr(arr);
    }
    if (e instanceof UnaryExpr ue) {
      Value rhs = eval(ue.expr(), env);
      if (ue.op() == UnaryOp.NEG) {
        if (rhs.kind != Value.Kind.NUMBER) throw err("Unary '-' expects a number", ue.span());
        return Value.num(-rhs.number);
      }
      throw err("Unknown unary operator", ue.span());
    }
    if (e instanceof BinaryExpr be) return evalBinary(be, env);
    if (e instanceof IndexExpr ie) {
      Value t = eval(ie.target(), env);
      Value idx = eval(ie.index(), env);
      if (t.kind != Value.Kind.ARRAY) throw err("Indexing with 'at' expects an array", ie.span());
      if (idx.kind != Value.Kind.NUMBER) throw err("Array index must be a number", ie.span());
      long i = Math.round(idx.number);
      if (i < 0 || i >= t.array.size()) throw err("Array index out of bounds", ie.span());
      return t.array.get((int) i);
    }
    if (e instanceof CallExpr ce) return evalCall(ce, env);
    throw err("Unknown expression", e.span());
  }

  private Value evalBinary(BinaryExpr be, Env env) {
    Value l = eval(be.left(), env);
    Value r = eval(be.right(), env);
    return switch (be.op()) {
      case ADD -> {
        if (l.kind == Value.Kind.NUMBER && r.kind == Value.Kind.NUMBER) yield Value.num(l.number + r.number);
        if (l.kind == Value.Kind.STRING || r.kind == Value.Kind.STRING) yield Value.str(Value.toDisplay(l) + Value.toDisplay(r));
        throw err("Operator '+' expects numbers or strings", be.span());
      }
      case SUB -> {
        if (l.kind != Value.Kind.NUMBER || r.kind != Value.Kind.NUMBER) throw err("Operator '-' expects numbers", be.span());
        yield Value.num(l.number - r.number);
      }
      case MUL -> {
        if (l.kind != Value.Kind.NUMBER || r.kind != Value.Kind.NUMBER) throw err("Operator '*' expects numbers", be.span());
        yield Value.num(l.number * r.number);
      }
      case DIV -> {
        if (l.kind != Value.Kind.NUMBER || r.kind != Value.Kind.NUMBER) throw err("Operator '/' expects numbers", be.span());
        if (Math.abs(r.number) < 1e-12) throw err("Division by zero", be.span());
        yield Value.num(l.number / r.number);
      }
      case EQ -> Value.bol(Value.equalsValue(l, r));
      case NEQ -> Value.bol(!Value.equalsValue(l, r));
      case GT, LT, GTE, LTE -> {
        if (l.kind != Value.Kind.NUMBER || r.kind != Value.Kind.NUMBER) throw err("Comparison expects numbers", be.span());
        boolean ok = switch (be.op()) {
          case GT -> l.number > r.number;
          case LT -> l.number < r.number;
          case GTE -> l.number >= r.number;
          case LTE -> l.number <= r.number;
          default -> false;
        };
        yield Value.bol(ok);
      }
    };
  }

  private Value evalCall(CallExpr ce, Env env) {
    Holder<Value> h = new Holder<>();
    if (!env.get(ce.callee(), h)) throw err("Undefined function: " + ce.callee(), ce.span());
    Value callee = h.value;
    if (callee.kind != Value.Kind.FUNCTION) throw err("Attempted to call a non-function: " + ce.callee(), ce.span());

    FunctionValue fn = callee.function;
    if (fn.params.size() != ce.args().size()) {
      throw err("Function '" + ce.callee() + "' expects " + fn.params.size() + " arguments, got " + ce.args().size(), ce.span());
    }

    Env callEnv = new Env(fn.closure);
    for (int i = 0; i < fn.params.size(); i++) {
      callEnv.setLocal(fn.params.get(i), eval(ce.args().get(i), env));
    }

    try {
      for (Stmt st : fn.body) execStmt(st, callEnv);
    } catch (ReturnSignal rs) {
      return rs.value;
    }
    return Value.nul();
  }

  private void execStmt(Stmt st, Env env) {
    if (st instanceof SetStmt ss) {
      Value v = eval(ss.value(), env);
      if (!env.assign(ss.name(), v)) env.setLocal(ss.name(), v);
      return;
    }
    if (st instanceof SayStmt ss) {
      Value v = eval(ss.value(), env);
      out.println(Value.toDisplay(v));
      return;
    }
    if (st instanceof AskStmt as) {
      Value prompt = eval(as.prompt(), env);
      out.print(Value.toDisplay(prompt) + ": ");
      out.flush();
      String line;
      try {
        line = in.readLine();
      } catch (IOException e) {
        line = "";
      }
      if (line == null) line = "";
      Value v = Value.str(line);
      if (!env.assign(as.intoName(), v)) env.setLocal(as.intoName(), v);
      return;
    }
    if (st instanceof IfStmt is) {
      for (IfBranch br : is.branches()) {
        Value cond = eval(br.condition, env);
        if (Value.truthy(cond)) {
          Env inner = new Env(env);
          for (Stmt s : br.body) execStmt(s, inner);
          return;
        }
      }
      if (is.elseBody() != null && !is.elseBody().isEmpty()) {
        Env inner = new Env(env);
        for (Stmt s : is.elseBody()) execStmt(s, inner);
      }
      return;
    }
    if (st instanceof WhileStmt ws) {
      while (Value.truthy(eval(ws.condition(), env))) {
        Env inner = new Env(env);
        for (Stmt s : ws.body()) execStmt(s, inner);
      }
      return;
    }
    if (st instanceof TryCatchStmt ts) {
      try {
        Env inner = new Env(env);
        for (Stmt s : ts.tryBody()) execStmt(s, inner);
      } catch (RuntimeError re) {
        Env inner = new Env(env);
        for (Stmt s : ts.catchBody()) execStmt(s, inner);
      }
      return;
    }
    if (st instanceof FunctionStmt fs) {
      // Store function AST directly (Java has GC; safe to reference).
      FunctionValue fn = new FunctionValue(fs.params(), fs.body(), env, fs.span());
      env.setLocal(fs.name(), Value.fun(fn));
      return;
    }
    if (st instanceof ReturnStmt rs) {
      Value v = Value.nul();
      if (rs.valueOrNull() != null) v = eval(rs.valueOrNull(), env);
      throw new ReturnSignal(v);
    }
    if (st instanceof ExprStmt es) {
      eval(es.expr(), env);
      return;
    }

    throw err("Unknown statement", st.span());
  }
}

