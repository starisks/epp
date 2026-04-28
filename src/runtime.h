#pragma once

#include "ast.h"
#include "source.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace epp {

struct RuntimeError {
  std::string message;
  Span span;
};

struct Value;
struct Env;

using EnvPtr = std::shared_ptr<Env>;

struct FunctionValue {
  std::vector<std::string> params;
  std::vector<StmtPtr> body; // owned by FunctionValue
  EnvPtr closure;
  Span span;
};

struct Value {
  using Array = std::vector<Value>;
  using Func = std::shared_ptr<FunctionValue>;

  std::variant<std::monostate, double, std::string, bool, Array, Func> data;

  static Value null() { return Value{}; }
  static Value number(double v) {
    Value x;
    x.data = v;
    return x;
  }
  static Value str(std::string v) {
    Value x;
    x.data = std::move(v);
    return x;
  }
  static Value boolean(bool v) {
    Value x;
    x.data = v;
    return x;
  }
  static Value array(Array v) {
    Value x;
    x.data = std::move(v);
    return x;
  }
  static Value func(Func f) {
    Value x;
    x.data = std::move(f);
    return x;
  }
};

struct Env : std::enable_shared_from_this<Env> {
  std::unordered_map<std::string, Value> vars;
  EnvPtr parent;

  explicit Env(EnvPtr p = nullptr) : parent(std::move(p)) {}

  bool hasLocal(const std::string& name) const;
  bool get(const std::string& name, Value& out) const;
  void setLocal(const std::string& name, Value v);
  bool assign(const std::string& name, Value v);
};

struct ExecResult {
  bool ok = true;
  std::vector<RuntimeError> errors;
};

ExecResult runProgram(const std::vector<StmtPtr>& program, std::istream& in, std::ostream& out);

std::string toString(const Value& v);
bool isTruthy(const Value& v);

} // namespace epp

