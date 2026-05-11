#include "runtime.h"
#include "lexer.h"
#include "parser.h"

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
static bool isModuleNs(const Value& v) { return std::holds_alternative<Value::ModuleNs>(v.data); }

static const char* typeName(const Value& v) {
  if (isNull(v)) return "null";
  if (isNumber(v)) return "number";
  if (isString(v)) return "string";
  if (isBool(v)) return "bool";
  if (isArray(v)) return "array";
  if (isModuleNs(v)) return "module";
  return "function";
}

static int compareNumbers(double a, double b) {
  if (std::isnan(a) || std::isnan(b)) return 2; // signal invalid
  if (Value::almostEqual(a, b)) return 0;
  return (a < b) ? -1 : 1;
}

void printError(const RuntimeError& err) {
  std::cerr << "Stack Traceback:\n";
  std::cerr << "[E++] " << err.message << "\n";
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
  if (isModuleNs(v)) {
    const auto& ns = std::get<Value::ModuleNs>(v.data);
    if (ns && ns->module) {
      return "<module " + ns->module->name + ">";
    }
    return "<module>";
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
  Interpreter(std::istream& in, std::ostream& out,
              std::shared_ptr<ModuleCache> moduleCache = nullptr)
      : in_(in), out_(out), moduleCache_(std::move(moduleCache)) {
    globals_ = std::make_shared<Env>();
    if (!moduleCache_) {
      moduleCache_ = std::make_shared<ModuleCache>();
    }
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
  std::shared_ptr<ModuleCache> moduleCache_;
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
        if (!env->get(ve.name, v)) throwErr("Undefined variable '" + ve.name + "'", ve.span);
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
      case ExprKind::MethodCall: return evalMethodCall(static_cast<const MethodCallExpr&>(e), env);
      case ExprKind::Dot: {
        const auto& de = static_cast<const DotExpr&>(e);
        Value left = evalExpr(*de.left, env);
        if (!isModuleNs(left)) {
          throwErr("Cannot access property on non-module value", de.span);
        }
        const auto& ns = std::get<Value::ModuleNs>(left.data);
        Value result;
        if (!ns->get(de.right, result)) {
          throwErr("Module has no export named '" + de.right + "'", de.span);
        }
        return result;
      }
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
        throwErr(std::string("Cannot add ") + typeName(l) + " and " + typeName(r), be.span);
      }
      case BinaryOp::Sub: {
        if (!isNumber(l) || !isNumber(r))
          throwErr(std::string("Cannot subtract ") + typeName(r) + " from " + typeName(l), be.span);
        return Value::number(std::get<double>(l.data) - std::get<double>(r.data));
      }
      case BinaryOp::Mul: {
        if (!isNumber(l) || !isNumber(r))
          throwErr(std::string("Cannot multiply ") + typeName(l) + " and " + typeName(r), be.span);
        return Value::number(std::get<double>(l.data) * std::get<double>(r.data));
      }
      case BinaryOp::Div: {
        if (!isNumber(l) || !isNumber(r))
          throwErr(std::string("Cannot divide ") + typeName(l) + " by " + typeName(r), be.span);
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
          throwErr(std::string("Cannot compare ") + typeName(l) + " and " + typeName(r), be.span);
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
      if (!env->get(var.name, arrV)) throwErr("Undefined variable '" + var.name + "'", ce.span);
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
      if (!env->get(var.name, arrV)) throwErr("Undefined variable '" + var.name + "'", ce.span);
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
    if (!env->get(ce.callee, calleeV)) throwErr("Undefined function '" + ce.callee + "'", ce.span);
    if (!isFunc(calleeV)) throwErr("Cannot call '" + ce.callee + "' (not a function)", ce.span);

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

  Value evalMethodCall(const MethodCallExpr& mc, const EnvPtr& env) {
    // Evaluate the receiver (should be a module namespace)
    Value receiver = evalExpr(*mc.receiver, env);
    if (!isModuleNs(receiver)) {
      throwErr("Cannot call method on non-module value", mc.span);
    }

    // Get the method from the module
    const auto& ns = std::get<Value::ModuleNs>(receiver.data);
    Value methodVal;
    if (!ns->get(mc.method, methodVal)) {
      throwErr("Module has no method named '" + mc.method + "'", mc.span);
    }

    if (!isFunc(methodVal)) {
      throwErr("'" + mc.method + "' is not a function", mc.span);
    }

    auto fn = std::get<Value::Func>(methodVal.data);
    if (!fn) throwErr("Invalid function", mc.span);

    // Evaluate arguments
    std::vector<Value> argVals;
    for (const auto& arg : mc.args) {
      argVals.push_back(evalExpr(*arg, env));
    }

    if (fn->params.size() != argVals.size()) {
      throwErr("Function '" + mc.method + "' expects " + std::to_string(fn->params.size()) +
                   " arguments, got " + std::to_string(argVals.size()),
               mc.span);
    }

    // Create call environment with function's closure
    EnvPtr callEnv = std::make_shared<Env>(fn->closure);
    for (size_t i = 0; i < fn->params.size(); i++) {
      callEnv->setLocal(fn->params[i], argVals[i]);
    }

    callStack_.push_back({mc.method, mc.span});
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
      case StmtKind::Import: {
        execImport(static_cast<const ImportStmt&>(st), env);
        return;
      }
      case StmtKind::Export: {
        execExport(static_cast<const ExportStmt&>(st), env);
        return;
      }
    }
    throwErr("Unknown statement", st.span);
  }

  // Import statement execution
  void execImport(const ImportStmt& is, const EnvPtr& env) {
    // Build module name
    std::string moduleName;
    for (size_t i = 0; i < is.modulePath.size(); ++i) {
      if (i > 0) moduleName += ".";
      moduleName += is.modulePath[i];
    }

    // Check if module is already cached
    ModulePtr mod = moduleCache_ ? moduleCache_->get(moduleName) : nullptr;

    if (!mod) {
      // Load and execute the module
      mod = loadAndExecuteModule(is.modulePath);
      if (moduleCache_ && mod) {
        moduleCache_->set(moduleName, mod);
      }
    }

    if (!mod || !mod->loaded) {
      throwErr("Failed to load module: " + moduleName, is.span);
    }

    // Handle different import styles
    if (!is.symbols.empty()) {
      // 'from module import symbol1, symbol2'
      for (const auto& sym : is.symbols) {
        Value val;
        if (!mod->getExport(sym, val)) {
          throwErr("Module '" + moduleName + "' does not export '" + sym + "'", is.span);
        }
        env->setLocal(sym, val);
      }
    } else if (is.importAll) {
      // 'from module import *' - import all exports into current scope
      for (const auto& [name, val] : mod->exports) {
        env->setLocal(name, val);
      }
    } else {
      // 'import module' or 'import module as alias'
      std::string varName = is.alias.empty() ? is.modulePath.back() : is.alias;
      auto ns = std::make_shared<ModuleNamespace>(mod);
      env->setLocal(varName, Value::moduleNs(ns));
    }
  }

  // Export statement execution
  void execExport(const ExportStmt& es, const EnvPtr& env) {
    // Exports are collected from the module's environment
    // For now, we don't need to do anything special at runtime
    // The exports are set up when the module is executed
    (void)es;
    (void)env;
  }

  // Load and execute a module
  ModulePtr loadAndExecuteModule(const std::vector<std::string>& modulePath) {
    // This is a placeholder - real implementation would integrate with ModuleLoader
    // For now, we'll support stdlib modules directly
    if (modulePath.empty()) return nullptr;

    if (modulePath[0] == "std" && modulePath.size() == 2) {
      return loadStdlibModule(modulePath[1]);
    }

    return nullptr;
  }

  // Load a stdlib module by name
  ModulePtr loadStdlibModule(const std::string& name) {
    // Build full module name
    std::string moduleName = "std." + name;

    // Check cache
    if (moduleCache_) {
      if (auto cached = moduleCache_->get(moduleName)) {
        return cached;
      }
    }

    auto mod = std::make_shared<Module>(moduleName, "");
    mod->loading = true;

    // Register in cache early for circular detection
    if (moduleCache_) {
      moduleCache_->set(moduleName, mod);
    }

    // Load stdlib module content
    std::string source = getStdlibSource(name);
    if (source.empty()) {
      mod->loading = false;
      return nullptr;
    }

    // Parse the source
    Source src{moduleName, source};
    auto lr = lex(src);
    if (!lr.errors.empty()) {
      mod->loading = false;
      return nullptr;
    }

    auto pr = parse(lr.tokens);
    if (!pr.errors.empty()) {
      mod->loading = false;
      return nullptr;
    }

    mod->ast = std::move(pr.program);

    // Execute module to populate exports
    executeModule(mod);

    return mod;
  }

  // Execute a module's AST and populate its exports
  void executeModule(ModulePtr mod) {
    if (!mod || mod->loaded || mod->loading == false) return;

    // Create module environment
    auto modEnv = std::make_shared<Env>(globals_);

    // Execute all statements
    for (const auto& st : mod->ast) {
      // Handle function definitions - they get added to modEnv
      if (st->kind == StmtKind::Function) {
        const auto& fs = static_cast<const FunctionStmt&>(*st);
        auto fn = std::make_shared<FunctionValue>();
        fn->params = fs.params;
        fn->closure = modEnv;
        fn->span = fs.span;
        fn->body = cloneBlock(fs.body);
        modEnv->setLocal(fs.name, Value::func(fn));
      }
      // Handle export statements
      else if (st->kind == StmtKind::Export) {
        const auto& es = static_cast<const ExportStmt&>(*st);
        for (const auto& sym : es.symbols) {
          Value val;
          if (modEnv->get(sym, val)) {
            mod->exports[sym] = val;
          }
        }
      }
      // Handle other statements
      else {
        execStmt(*st, modEnv);
      }
    }

    // Also export any top-level functions that weren't explicitly exported
    // (if no export statements, export everything)
    bool hasExplicitExport = false;
    for (const auto& st : mod->ast) {
      if (st->kind == StmtKind::Export) {
        hasExplicitExport = true;
        break;
      }
    }

    if (!hasExplicitExport) {
      // Export all top-level bindings
      for (const auto& [name, val] : modEnv->vars) {
        mod->exports[name] = val;
      }
    }

    mod->loaded = true;
    mod->loading = false;
  }

  // Get stdlib module source code
  std::string getStdlibSource(const std::string& name) {
    if (name == "math") {
      return R"(
function add a and b
  return a + b
end

function subtract a and b
  return a - b
end

function multiply a and b
  return a * b
end

function divide a and b
  return a / b
end

function power base and exp
  set result to 1
  set i to 0
  while i is less than exp do
    set result to result * base
    set i to i + 1
  end
  return result
end

function abs x
  if x is less than 0 then
    return -x
  end
  return x
end

function max a and b
  if a is greater than b then
    return a
  end
  return b
end

function min a and b
  if a is less than b then
    return a
  end
  return b
end
)";
    }
    if (name == "io") {
      return R"(
# io module - input/output utilities
# Note: print and input are handled specially by the runtime

export { print, println, input }
)";
    }
    if (name == "collections") {
      return R"(
function map arr and fn
  set result to []
  set i to 0
  while i is less than len arr do
    set val to fn arr at i
    push result and val
    set i to i + 1
  end
  return result
end

function filter arr and fn
  set result to []
  set i to 0
  while i is less than len arr do
    set val to arr at i
    if fn val then
      push result and val
    end
    set i to i + 1
  end
  return result
end

function reduce arr and fn and init
  set acc to init
  set i to 0
  while i is less than len arr do
    set val to arr at i
    set acc to fn acc and val
    set i to i + 1
  end
  return acc
end

function find arr and fn
  set i to 0
  while i is less than len arr do
    set val to arr at i
    if fn val then
      return val
    end
    set i to i + 1
  end
  return null
end

function contains arr and value
  set i to 0
  while i is less than len arr do
    set val to arr at i
    if val is equal to value then
      return true
    end
    set i to i + 1
  end
  return false
end
)";
    }
    if (name == "sys") {
      return R"(
# sys module - system utilities

function clock
  # Returns current time as seconds since epoch
  # Placeholder - would integrate with system clock
  return 0
end

function exit code
  # Would exit program with given code
  # Placeholder
end

export { clock, exit }
)";
    }
    return "";
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
      case ExprKind::MethodCall: {
        const auto& mc = static_cast<const MethodCallExpr&>(e);
        std::vector<ExprPtr> args;
        args.reserve(mc.args.size());
        for (const auto& a : mc.args) args.push_back(cloneExpr(*a));
        return std::make_unique<MethodCallExpr>(cloneExpr(*mc.receiver), mc.method, std::move(args), e.span);
      }
      case ExprKind::Index: {
        const auto& ie = static_cast<const IndexExpr&>(e);
        return std::make_unique<IndexExpr>(cloneExpr(*ie.target), cloneExpr(*ie.index), e.span);
      }
      case ExprKind::Group: {
        const auto& ge = static_cast<const GroupExpr&>(e);
        return std::make_unique<GroupExpr>(cloneExpr(*ge.inner), e.span);
      }
      case ExprKind::Dot: {
        const auto& de = static_cast<const DotExpr&>(e);
        return std::make_unique<DotExpr>(cloneExpr(*de.left), de.right, e.span);
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
      case StmtKind::Import: {
        const auto& is = static_cast<const ImportStmt&>(st);
        return std::make_unique<ImportStmt>(is.modulePath, is.symbols, is.importAll, st.span);
      }
      case StmtKind::Export: {
        const auto& es = static_cast<const ExportStmt&>(st);
        return std::make_unique<ExportStmt>(es.symbols, st.span);
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

ExecResult runProgramWithModules(const std::vector<StmtPtr>& program, std::istream& in, std::ostream& out,
                                 std::shared_ptr<ModuleCache> moduleCache) {
  Interpreter it(in, out, moduleCache);
  return it.exec(program);
}

// ModuleCache implementation
ModulePtr ModuleCache::get(const std::string& modulePath) const {
  auto it = modules_.find(modulePath);
  if (it != modules_.end()) return it->second;
  return nullptr;
}

void ModuleCache::set(const std::string& modulePath, ModulePtr module) {
  modules_[modulePath] = std::move(module);
}

bool ModuleCache::isLoading(const std::string& modulePath) const {
  auto mod = get(modulePath);
  return mod && mod->loading;
}

void ModuleCache::clear() {
  modules_.clear();
}

// Module implementation
bool Module::getExport(const std::string& name, Value& out) const {
  auto it = exports.find(name);
  if (it != exports.end()) {
    out = it->second;
    return true;
  }
  return false;
}

// ModuleNamespace implementation
bool ModuleNamespace::get(const std::string& name, Value& out) const {
  auto it = bindings.find(name);
  if (it != bindings.end()) {
    out = it->second;
    return true;
  }
  // Fall back to module exports if module is loaded
  if (module && module->loaded) {
    return module->getExport(name, out);
  }
  return false;
}

} // namespace epp

