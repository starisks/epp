#include "runtime.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace epp {

bool Env::hasLocal(const std::string& name) const { return vars.find(name) != vars.end(); }

bool Env::get(const std::string& name, Value& out) const {
  auto it = vars.find(name);
  if (it != vars.end()) {
    out = it->second;
    return true;
  }
  if (parent) return parent->get(name, out);
  return false;
}

void Env::setLocal(const std::string& name, Value v) { vars[name] = std::move(v); }

bool Env::assign(const std::string& name, Value v) {
  auto it = vars.find(name);
  if (it != vars.end()) {
    it->second = std::move(v);
    return true;
  }
  if (parent) return parent->assign(name, std::move(v));
  return false;
}

static bool isNull(const Value& v) { return std::holds_alternative<std::monostate>(v.data); }
static bool isNumber(const Value& v) { return std::holds_alternative<double>(v.data); }
static bool isString(const Value& v) { return std::holds_alternative<std::string>(v.data); }
static bool isBool(const Value& v) { return std::holds_alternative<bool>(v.data); }
static bool isArray(const Value& v) { return std::holds_alternative<Value::Array>(v.data); }
static bool isFunc(const Value& v) { return std::holds_alternative<Value::Func>(v.data); }

static const char* typeName(const Value& v) {
  if (isNull(v)) return "null";
  if (isNumber(v)) return "number";
  if (isString(v)) return "string";
  if (isBool(v)) return "bool";
  if (isArray(v)) return "array";
  return "function";
}

static int compareNumbers(double a, double b) {
  if (std::isnan(a) || std::isnan(b)) return 2; // signal invalid
  if (Value::almostEqual(a, b)) return 0;
  return (a < b) ? -1 : 1;
}

std::string toString(const Value& v) {
  if (isNull(v)) return "null";
  if (isNumber(v)) {
    double d = std::get<double>(v.data);
    std::ostringstream oss;
    if (std::fabs(d - std::round(d)) < 1e-9) {
      oss << static_cast<long long>(std::llround(d));
    } else {
      oss << d;
    }
    return oss.str();
  }
  if (isString(v)) return std::get<std::string>(v.data);
  if (isBool(v)) return std::get<bool>(v.data) ? "true" : "false";
  if (isArray(v)) {
    const auto& a = std::get<Value::Array>(v.data);
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < a.size(); i++) {
      if (i) oss << ", ";
      oss << toString(a[i]);
    }
    oss << "]";
    return oss.str();
  }
  return "<function>";
}

bool isTruthy(const Value& v) {
  if (isNull(v)) return false;
  if (isBool(v)) return std::get<bool>(v.data);
  if (isNumber(v)) return std::get<double>(v.data) != 0.0;
  if (isString(v)) return !std::get<std::string>(v.data).empty();
  if (isArray(v)) return !std::get<Value::Array>(v.data).empty();
  return true;
}

struct ReturnSignal {
  Value value;
};

class Interpreter {
 public:
  Interpreter(std::istream& in, std::ostream& out) : in_(in), out_(out) {
    globals_ = std::make_shared<Env>();
  }

  ExecResult exec(const std::vector<StmtPtr>& program) {
    ExecResult res;
    try {
      for (const auto& st : program) execStmt(*st, globals_);
    } catch (const RuntimeError& e) {
      res.ok = false;
      res.errors.push_back(e);
    }
    return res;
  }

 private:
  std::istream& in_;
  std::ostream& out_;
  EnvPtr globals_;
  std::vector<std::pair<std::string, Span>> callStack_;

  [[noreturn]] void throwErr(const std::string& msg, Span sp) {
    std::string full = msg;
    if (!callStack_.empty()) {
      full += "\nStack trace:";
      for (auto it = callStack_.rbegin(); it != callStack_.rend(); ++it) {
        full += "\n  at " + it->first + " (line " + std::to_string(it->second.line) + ", col " +
                std::to_string(it->second.col) + ")";
      }
    }
    throw RuntimeError{full, sp};
  }

  Value evalExpr(const Expr& e, const EnvPtr& env) {
    switch (e.kind) {
      case ExprKind::Number: return Value::number(static_cast<const NumberExpr&>(e).value);
      case ExprKind::String: return Value::str(static_cast<const StringExpr&>(e).value);
      case ExprKind::Bool: return Value::boolean(static_cast<const BoolExpr&>(e).value);
      case ExprKind::Var: {
        const auto& ve = static_cast<const VarExpr&>(e);
        Value v;
        if (!env->get(ve.name, v)) throwErr("Undefined variable: " + ve.name, ve.span);
        return v;
      }
      case ExprKind::Array: {
        const auto& ae = static_cast<const ArrayExpr&>(e);
        Value::Array arr;
        arr.reserve(ae.elements.size());
        for (const auto& el : ae.elements) arr.push_back(evalExpr(*el, env));
        return Value::array(std::move(arr));
      }
      case ExprKind::Group: {
        const auto& ge = static_cast<const GroupExpr&>(e);
        return evalExpr(*ge.inner, env);
      }
      case ExprKind::Unary: {
        const auto& ue = static_cast<const UnaryExpr&>(e);
        Value rhs = evalExpr(*ue.expr, env);
        if (ue.op == UnaryOp::Neg) {
          if (!isNumber(rhs)) throwErr("Unary '-' expects a number", ue.span);
          return Value::number(-std::get<double>(rhs.data));
        }
        if (ue.op == UnaryOp::Not) {
          return Value::boolean(!isTruthy(rhs));
        }
        throwErr("Unknown unary operator", ue.span);
      }
      case ExprKind::Binary: return evalBinary(static_cast<const BinaryExpr&>(e), env);
      case ExprKind::Index: {
        const auto& ie = static_cast<const IndexExpr&>(e);
        Value t = evalExpr(*ie.target, env);
        Value idxV = evalExpr(*ie.index, env);
        if (!isArray(t)) throwErr("Indexing with 'at' expects an array", ie.span);
        if (!isNumber(idxV)) throwErr("Array index must be a number", ie.span);
        const auto& arr = std::get<Value::Array>(t.data);
        long long idx = static_cast<long long>(std::llround(std::get<double>(idxV.data)));
        if (idx < 0 || static_cast<size_t>(idx) >= arr.size())
          throwErr("Array index out of bounds", ie.span);
        return arr[static_cast<size_t>(idx)];
      }
      case ExprKind::Call: return evalCall(static_cast<const CallExpr&>(e), env);
    }
    throwErr("Unknown expression", e.span);
  }

  Value evalBinary(const BinaryExpr& be, const EnvPtr& env) {
    if (be.op == BinaryOp::LogicalAnd) {
      Value l = evalExpr(*be.left, env);
      if (!isTruthy(l)) return Value::boolean(false);
      Value r = evalExpr(*be.right, env);
      return Value::boolean(isTruthy(r));
    }
    if (be.op == BinaryOp::LogicalOr) {
      Value l = evalExpr(*be.left, env);
      if (isTruthy(l)) return Value::boolean(true);
      Value r = evalExpr(*be.right, env);
      return Value::boolean(isTruthy(r));
    }

    Value l = evalExpr(*be.left, env);
    Value r = evalExpr(*be.right, env);

    switch (be.op) {
      case BinaryOp::Add: {
        if (isNumber(l) && isNumber(r)) return Value::number(std::get<double>(l.data) + std::get<double>(r.data));
        if (isString(l) || isString(r)) return Value::str(toString(l) + toString(r));
        throwErr(std::string("Operator '+' expects numbers or strings (got ") + typeName(l) + " and " +
                     typeName(r) + ")",
                 be.span);
      }
      case BinaryOp::Sub: {
        if (!isNumber(l) || !isNumber(r))
          throwErr(std::string("Operator '-' expects numbers (got ") + typeName(l) + " and " + typeName(r) + ")",
                   be.span);
        return Value::number(std::get<double>(l.data) - std::get<double>(r.data));
      }
      case BinaryOp::Mul: {
        if (!isNumber(l) || !isNumber(r))
          throwErr(std::string("Operator '*' expects numbers (got ") + typeName(l) + " and " + typeName(r) + ")",
                   be.span);
        return Value::number(std::get<double>(l.data) * std::get<double>(r.data));
      }
      case BinaryOp::Div: {
        if (!isNumber(l) || !isNumber(r))
          throwErr(std::string("Operator '/' expects numbers (got ") + typeName(l) + " and " + typeName(r) + ")",
                   be.span);
        double denom = std::get<double>(r.data);
        if (std::fabs(denom) < 1e-12) throwErr("Division by zero", be.span);
        return Value::number(std::get<double>(l.data) / denom);
      }
      case BinaryOp::Eq: return Value::boolean(l == r);
      case BinaryOp::Neq: return Value::boolean(!(l == r));
      case BinaryOp::Gt:
      case BinaryOp::Lt:
      case BinaryOp::Gte:
      case BinaryOp::Lte: {
        if (!isNumber(l) || !isNumber(r))
          throwErr(std::string("Comparison expects numbers (got ") + typeName(l) + " and " + typeName(r) + ")",
                   be.span);
        double a = std::get<double>(l.data);
        double b = std::get<double>(r.data);
        int c = compareNumbers(a, b);
        if (c == 2) throwErr("Cannot compare NaN", be.span);

        if (be.op == BinaryOp::Gt) return Value::boolean(c > 0);
        if (be.op == BinaryOp::Lt) return Value::boolean(c < 0);
        if (be.op == BinaryOp::Gte) return Value::boolean(c >= 0);
        return Value::boolean(c <= 0);
      }
      case BinaryOp::LogicalAnd:
      case BinaryOp::LogicalOr:
        break; // handled above
    }
    throwErr("Unknown binary operator", be.span);
  }

  Value evalCall(const CallExpr& ce, const EnvPtr& env) {
    // ---- built-ins (kept minimal and non-invasive) ----
    if (ce.callee == "len") {
      if (ce.args.size() != 1) {
        throwErr("Built-in 'len' expects 1 argument, got " + std::to_string(ce.args.size()), ce.span);
      }
      Value v = evalExpr(*ce.args[0], env);
      if (isArray(v)) return Value::number(static_cast<double>(std::get<Value::Array>(v.data).size()));
      if (isString(v)) return Value::number(static_cast<double>(std::get<std::string>(v.data).size()));
      throwErr(std::string("Built-in 'len' expects array or string (got ") + typeName(v) + ")", ce.span);
    }

    if (ce.callee == "type") {
      if (ce.args.size() != 1) {
        throwErr("Built-in 'type' expects 1 argument, got " + std::to_string(ce.args.size()), ce.span);
      }
      Value v = evalExpr(*ce.args[0], env);
      return Value::str(std::string(typeName(v)));
    }

    if (ce.callee == "push") {
      if (ce.args.size() != 2) {
        throwErr("Built-in 'push' expects 2 arguments, got " + std::to_string(ce.args.size()), ce.span);
      }
      const Expr* firstArg = ce.args[0].get();
      if (!firstArg || firstArg->kind != ExprKind::Var) {
        throwErr("Built-in 'push' expects a variable as its first argument", ce.span);
      }
      const auto& var = static_cast<const VarExpr&>(*firstArg);
      Value arrV;
      if (!env->get(var.name, arrV)) throwErr("Undefined variable: " + var.name, ce.span);
      if (!isArray(arrV)) throwErr("Built-in 'push' expects an array variable (got " + std::string(typeName(arrV)) + ")", ce.span);

      Value valueToPush = evalExpr(*ce.args[1], env);
      auto arr = std::get<Value::Array>(arrV.data);
      arr.push_back(valueToPush);
      Value outArr = Value::array(std::move(arr));
      if (!env->assign(var.name, outArr)) env->setLocal(var.name, outArr);
      return outArr;
    }

    if (ce.callee == "pop") {
      if (ce.args.size() != 1) {
        throwErr("Built-in 'pop' expects 1 argument, got " + std::to_string(ce.args.size()), ce.span);
      }
      const Expr* firstArg = ce.args[0].get();
      if (!firstArg || firstArg->kind != ExprKind::Var) {
        throwErr("Built-in 'pop' expects a variable as its argument", ce.span);
      }
      const auto& var = static_cast<const VarExpr&>(*firstArg);
      Value arrV;
      if (!env->get(var.name, arrV)) throwErr("Undefined variable: " + var.name, ce.span);
      if (!isArray(arrV)) throwErr("Built-in 'pop' expects an array variable (got " + std::string(typeName(arrV)) + ")", ce.span);

      auto arr = std::get<Value::Array>(arrV.data);
      if (arr.empty()) throwErr("Built-in 'pop' cannot pop from empty array", ce.span);
      Value popped = arr.back();
      arr.pop_back();
      Value outArr = Value::array(std::move(arr));
      if (!env->assign(var.name, outArr)) env->setLocal(var.name, outArr);
      return popped;
    }

    Value calleeV;
    if (!env->get(ce.callee, calleeV)) throwErr("Undefined function: " + ce.callee, ce.span);
    if (!isFunc(calleeV)) throwErr("Attempted to call a non-function: " + ce.callee, ce.span);

    auto fn = std::get<Value::Func>(calleeV.data);
    if (!fn) throwErr("Invalid function", ce.span);
    if (fn->params.size() != ce.args.size()) {
      throwErr("Function '" + ce.callee + "' expects " + std::to_string(fn->params.size()) +
                   " arguments, got " + std::to_string(ce.args.size()),
               ce.span);
    }

    EnvPtr callEnv = std::make_shared<Env>(fn->closure);
    for (size_t i = 0; i < fn->params.size(); i++) {
      callEnv->setLocal(fn->params[i], evalExpr(*ce.args[i], env));
    }

    callStack_.push_back({ce.callee, ce.span});
    try {
      for (const auto& st : fn->body) execStmt(*st, callEnv);
    } catch (const ReturnSignal& rs) {
      callStack_.pop_back();
      return rs.value;
    } catch (...) {
      callStack_.pop_back();
      throw;
    }
    callStack_.pop_back();
    return Value::null();
  }

  void execStmt(const Stmt& st, const EnvPtr& env) {
    switch (st.kind) {
      case StmtKind::Set: {
        const auto& ss = static_cast<const SetStmt&>(st);
        Value v = evalExpr(*ss.value, env);
        // Ruby-like: assignment updates nearest scope if already defined; otherwise local.
        if (!env->assign(ss.name, v)) env->setLocal(ss.name, v);
        return;
      }
      case StmtKind::Say: {
        const auto& ss = static_cast<const SayStmt&>(st);
        Value v = evalExpr(*ss.value, env);
        out_ << toString(v) << "\n";
        return;
      }
      case StmtKind::Ask: {
        const auto& as = static_cast<const AskStmt&>(st);
        Value prompt = evalExpr(*as.prompt, env);
        out_ << toString(prompt) << ": ";
        out_.flush();
        std::string line;
        if (!std::getline(in_, line)) line = "";
        if (!env->assign(as.intoName, Value::str(line))) env->setLocal(as.intoName, Value::str(line));
        return;
      }
      case StmtKind::If: {
        const auto& is = static_cast<const IfStmt&>(st);
        for (const auto& br : is.branches) {
          Value cond = evalExpr(*br.condition, env);
          if (isTruthy(cond)) {
            EnvPtr inner = std::make_shared<Env>(env);
            for (const auto& s : br.body) execStmt(*s, inner);
            return;
          }
        }
        if (!is.elseBody.empty()) {
          EnvPtr inner = std::make_shared<Env>(env);
          for (const auto& s : is.elseBody) execStmt(*s, inner);
        }
        return;
      }
      case StmtKind::While: {
        const auto& ws = static_cast<const WhileStmt&>(st);
        while (isTruthy(evalExpr(*ws.condition, env))) {
          EnvPtr inner = std::make_shared<Env>(env);
          for (const auto& s : ws.body) execStmt(*s, inner);
        }
        return;
      }
      case StmtKind::TryCatch: {
        const auto& ts = static_cast<const TryCatchStmt&>(st);
        try {
          EnvPtr inner = std::make_shared<Env>(env);
          for (const auto& s : ts.tryBody) execStmt(*s, inner);
        } catch (const RuntimeError&) {
          EnvPtr inner = std::make_shared<Env>(env);
          for (const auto& s : ts.catchBody) execStmt(*s, inner);
        }
        return;
      }
      case StmtKind::Function: {
        const auto& fs = static_cast<const FunctionStmt&>(st);
        auto fn = std::make_shared<FunctionValue>();
        fn->params = fs.params;
        fn->closure = env;
        fn->span = fs.span;
        // Deep-copy body AST pointers into the function object (owning).
        // For now, simplest strategy: move isn't possible (AST owned by program), so we clone by
        // serial ownership (we accept that function bodies share nodes by pointer stability?).
        // Instead of cloning, we rebuild by copying raw pointers would be unsafe. We'll do a
        // small trick: store references via shared ownership is non-trivial with unique_ptr.
        //
        // Production path: introduce shared_ptr AST or bytecode. For now: we re-parse per function
        // would be expensive. So we implement a conservative clone for supported statements.
        fn->body = cloneBlock(fs.body);
        env->setLocal(fs.name, Value::func(fn));
        return;
      }
      case StmtKind::Return: {
        const auto& rs = static_cast<const ReturnStmt&>(st);
        Value v = Value::null();
        if (rs.value) v = evalExpr(*rs.value, env);
        throw ReturnSignal{v};
      }
      case StmtKind::ExprStmt: {
        const auto& es = static_cast<const ExprStmt&>(st);
        (void)evalExpr(*es.expr, env);
        return;
      }
    }
    throwErr("Unknown statement", st.span);
  }

  // ---- cloning (temporary ownership solution) ----
  // Note: This is intentionally limited to our current AST. A real production path would switch
  // AST ownership model (shared) or compile to bytecode.

  static ExprPtr cloneExpr(const Expr& e) {
    switch (e.kind) {
      case ExprKind::Number: return std::make_unique<NumberExpr>(static_cast<const NumberExpr&>(e).value, e.span);
      case ExprKind::String: return std::make_unique<StringExpr>(static_cast<const StringExpr&>(e).value, e.span);
      case ExprKind::Bool: return std::make_unique<BoolExpr>(static_cast<const BoolExpr&>(e).value, e.span);
      case ExprKind::Var: return std::make_unique<VarExpr>(static_cast<const VarExpr&>(e).name, e.span);
      case ExprKind::Array: {
        const auto& ae = static_cast<const ArrayExpr&>(e);
        std::vector<ExprPtr> elems;
        elems.reserve(ae.elements.size());
        for (const auto& el : ae.elements) elems.push_back(cloneExpr(*el));
        return std::make_unique<ArrayExpr>(std::move(elems), e.span);
      }
      case ExprKind::Binary: {
        const auto& be = static_cast<const BinaryExpr&>(e);
        return std::make_unique<BinaryExpr>(be.op, cloneExpr(*be.left), cloneExpr(*be.right), e.span);
      }
      case ExprKind::Unary: {
        const auto& ue = static_cast<const UnaryExpr&>(e);
        return std::make_unique<UnaryExpr>(ue.op, cloneExpr(*ue.expr), e.span);
      }
      case ExprKind::Call: {
        const auto& ce = static_cast<const CallExpr&>(e);
        std::vector<ExprPtr> args;
        args.reserve(ce.args.size());
        for (const auto& a : ce.args) args.push_back(cloneExpr(*a));
        return std::make_unique<CallExpr>(ce.callee, std::move(args), e.span);
      }
      case ExprKind::Index: {
        const auto& ie = static_cast<const IndexExpr&>(e);
        return std::make_unique<IndexExpr>(cloneExpr(*ie.target), cloneExpr(*ie.index), e.span);
      }
      case ExprKind::Group: {
        const auto& ge = static_cast<const GroupExpr&>(e);
        return std::make_unique<GroupExpr>(cloneExpr(*ge.inner), e.span);
      }
    }
    return nullptr;
  }

  static StmtPtr cloneStmt(const Stmt& st) {
    switch (st.kind) {
      case StmtKind::Set: {
        const auto& ss = static_cast<const SetStmt&>(st);
        return std::make_unique<SetStmt>(ss.name, cloneExpr(*ss.value), st.span);
      }
      case StmtKind::Say: {
        const auto& ss = static_cast<const SayStmt&>(st);
        return std::make_unique<SayStmt>(cloneExpr(*ss.value), st.span);
      }
      case StmtKind::Ask: {
        const auto& as = static_cast<const AskStmt&>(st);
        return std::make_unique<AskStmt>(cloneExpr(*as.prompt), as.intoName, st.span);
      }
      case StmtKind::If: {
        const auto& is = static_cast<const IfStmt&>(st);
        std::vector<IfBranch> brs;
        brs.reserve(is.branches.size());
        for (const auto& br : is.branches) {
          IfBranch nb;
          nb.span = br.span;
          nb.condition = cloneExpr(*br.condition);
          nb.body = cloneBlock(br.body);
          brs.push_back(std::move(nb));
        }
        auto elseB = cloneBlock(is.elseBody);
        return std::make_unique<IfStmt>(std::move(brs), std::move(elseB), st.span);
      }
      case StmtKind::While: {
        const auto& ws = static_cast<const WhileStmt&>(st);
        return std::make_unique<WhileStmt>(cloneExpr(*ws.condition), cloneBlock(ws.body), st.span);
      }
      case StmtKind::TryCatch: {
        const auto& ts = static_cast<const TryCatchStmt&>(st);
        return std::make_unique<TryCatchStmt>(cloneBlock(ts.tryBody), cloneBlock(ts.catchBody), st.span);
      }
      case StmtKind::Function: {
        const auto& fs = static_cast<const FunctionStmt&>(st);
        return std::make_unique<FunctionStmt>(fs.name, fs.params, cloneBlock(fs.body), st.span);
      }
      case StmtKind::Return: {
        const auto& rs = static_cast<const ReturnStmt&>(st);
        return std::make_unique<ReturnStmt>(rs.value ? cloneExpr(*rs.value) : nullptr, st.span);
      }
      case StmtKind::ExprStmt: {
        const auto& es = static_cast<const ExprStmt&>(st);
        return std::make_unique<ExprStmt>(cloneExpr(*es.expr), st.span);
      }
    }
    return nullptr;
  }

  static std::vector<StmtPtr> cloneBlock(const std::vector<StmtPtr>& block) {
    std::vector<StmtPtr> out;
    out.reserve(block.size());
    for (const auto& st : block) out.push_back(cloneStmt(*st));
    return out;
  }
};

ExecResult runProgram(const std::vector<StmtPtr>& program, std::istream& in, std::ostream& out) {
  Interpreter it(in, out);
  return it.exec(program);
}

} // namespace epp

