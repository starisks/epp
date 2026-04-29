#pragma once

#include "ast.h"
#include "source.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>
#include <cmath>

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

  static bool almostEqual(double a, double b) {
    if (std::isnan(a) || std::isnan(b)) return false;
    if (std::isinf(a) || std::isinf(b)) return a == b;
    // Relative+absolute epsilon (more stable than a single absolute epsilon).
    const double diff = std::fabs(a - b);
    const double scale = std::max(1.0, std::max(std::fabs(a), std::fabs(b)));
    return diff <= 1e-9 * scale;
  }

  bool operator==(const Value& other) const {
    if (data.index() != other.data.index()) return false;

    if (std::holds_alternative<std::monostate>(data)) return true;

    if (std::holds_alternative<double>(data)) {
      return almostEqual(std::get<double>(data), std::get<double>(other.data));
    }

    if (std::holds_alternative<std::string>(data)) {
      return std::get<std::string>(data) == std::get<std::string>(other.data);
    }

    if (std::holds_alternative<bool>(data)) {
      return std::get<bool>(data) == std::get<bool>(other.data);
    }

    if (std::holds_alternative<Array>(data)) {
      const auto& a = std::get<Array>(data);
      const auto& b = std::get<Array>(other.data);
      if (a.size() != b.size()) return false;
      for (size_t i = 0; i < a.size(); i++) {
        if (!(a[i] == b[i])) return false;
      }
      return true;
    }

    // Functions compare by identity (same pointer).
    return std::get<Func>(data) == std::get<Func>(other.data);
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

