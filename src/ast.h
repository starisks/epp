#pragma once

#include "source.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace epp {

struct Expr;
struct Stmt;

using ExprPtr = std::unique_ptr<Expr>;
using StmtPtr = std::unique_ptr<Stmt>;

enum class ExprKind {
  Number,
  String,
  Bool,
  Var,
  Array,
  Binary,
  Unary,
  Call,
  MethodCall,  // obj.method args
  Index,
  Group,
  Dot,         // namespace access: module.symbol
};

enum class StmtKind {
  Set,
  Say,
  Ask,
  If,
  While,
  TryCatch,
  Function,
  Return,
  ExprStmt,
  Import,
  Export,
};

struct Expr {
  ExprKind kind;
  Span span;
  explicit Expr(ExprKind k, Span sp) : kind(k), span(sp) {}
  virtual ~Expr() = default;
};

struct NumberExpr final : Expr {
  double value;
  NumberExpr(double v, Span sp) : Expr(ExprKind::Number, sp), value(v) {}
};

struct StringExpr final : Expr {
  std::string value;
  StringExpr(std::string v, Span sp) : Expr(ExprKind::String, sp), value(std::move(v)) {}
};

struct BoolExpr final : Expr {
  bool value;
  BoolExpr(bool v, Span sp) : Expr(ExprKind::Bool, sp), value(v) {}
};

struct VarExpr final : Expr {
  std::string name;
  VarExpr(std::string n, Span sp) : Expr(ExprKind::Var, sp), name(std::move(n)) {}
};

struct ArrayExpr final : Expr {
  std::vector<ExprPtr> elements;
  ArrayExpr(std::vector<ExprPtr> elems, Span sp)
      : Expr(ExprKind::Array, sp), elements(std::move(elems)) {}
};

enum class BinaryOp {
  Add,
  Sub,
  Mul,
  Div,
  Eq,
  Neq,
  Gt,
  Lt,
  Gte,
  Lte,
  LogicalAnd,
  LogicalOr,
};

struct BinaryExpr final : Expr {
  BinaryOp op;
  ExprPtr left;
  ExprPtr right;
  BinaryExpr(BinaryOp o, ExprPtr l, ExprPtr r, Span sp)
      : Expr(ExprKind::Binary, sp), op(o), left(std::move(l)), right(std::move(r)) {}
};

enum class UnaryOp { Neg, Not };

struct UnaryExpr final : Expr {
  UnaryOp op;
  ExprPtr expr;
  UnaryExpr(UnaryOp o, ExprPtr e, Span sp)
      : Expr(ExprKind::Unary, sp), op(o), expr(std::move(e)) {}
};

struct CallExpr final : Expr {
  std::string callee;
  std::vector<ExprPtr> args;
  CallExpr(std::string c, std::vector<ExprPtr> a, Span sp)
      : Expr(ExprKind::Call, sp), callee(std::move(c)), args(std::move(a)) {}
};

// Method call: obj.method arg1 and arg2
struct MethodCallExpr final : Expr {
  ExprPtr receiver;        // The object/module
  std::string method;      // The method name
  std::vector<ExprPtr> args;
  MethodCallExpr(ExprPtr r, std::string m, std::vector<ExprPtr> a, Span sp)
      : Expr(ExprKind::MethodCall, sp), receiver(std::move(r)), method(std::move(m)), args(std::move(a)) {}
};

struct IndexExpr final : Expr {
  ExprPtr target;
  ExprPtr index;
  IndexExpr(ExprPtr t, ExprPtr i, Span sp)
      : Expr(ExprKind::Index, sp), target(std::move(t)), index(std::move(i)) {}
};

struct GroupExpr final : Expr {
  ExprPtr inner;
  GroupExpr(ExprPtr e, Span sp) : Expr(ExprKind::Group, sp), inner(std::move(e)) {}
};

// Namespace/module access: module.symbol
struct DotExpr final : Expr {
  ExprPtr left;    // The module/namespace
  std::string right;  // The symbol name
  DotExpr(ExprPtr l, std::string r, Span sp)
      : Expr(ExprKind::Dot, sp), left(std::move(l)), right(std::move(r)) {}
};

struct Stmt {
  StmtKind kind;
  Span span;
  explicit Stmt(StmtKind k, Span sp) : kind(k), span(sp) {}
  virtual ~Stmt() = default;
};

struct SetStmt final : Stmt {
  std::string name;
  ExprPtr value;
  SetStmt(std::string n, ExprPtr v, Span sp)
      : Stmt(StmtKind::Set, sp), name(std::move(n)), value(std::move(v)) {}
};

struct SayStmt final : Stmt {
  ExprPtr value;
  SayStmt(ExprPtr v, Span sp) : Stmt(StmtKind::Say, sp), value(std::move(v)) {}
};

struct AskStmt final : Stmt {
  ExprPtr prompt;
  std::string intoName;
  AskStmt(ExprPtr p, std::string into, Span sp)
      : Stmt(StmtKind::Ask, sp), prompt(std::move(p)), intoName(std::move(into)) {}
};

struct IfBranch {
  ExprPtr condition;
  std::vector<StmtPtr> body;
  Span span;
};

struct IfStmt final : Stmt {
  std::vector<IfBranch> branches; // if + else if
  std::vector<StmtPtr> elseBody;
  IfStmt(std::vector<IfBranch> b, std::vector<StmtPtr> e, Span sp)
      : Stmt(StmtKind::If, sp), branches(std::move(b)), elseBody(std::move(e)) {}
};

struct WhileStmt final : Stmt {
  ExprPtr condition;
  std::vector<StmtPtr> body;
  WhileStmt(ExprPtr c, std::vector<StmtPtr> b, Span sp)
      : Stmt(StmtKind::While, sp), condition(std::move(c)), body(std::move(b)) {}
};

struct TryCatchStmt final : Stmt {
  std::vector<StmtPtr> tryBody;
  std::vector<StmtPtr> catchBody;
  TryCatchStmt(std::vector<StmtPtr> t, std::vector<StmtPtr> c, Span sp)
      : Stmt(StmtKind::TryCatch, sp), tryBody(std::move(t)), catchBody(std::move(c)) {}
};

struct FunctionStmt final : Stmt {
  std::string name;
  std::vector<std::string> params;
  std::vector<StmtPtr> body;
  FunctionStmt(std::string n, std::vector<std::string> p, std::vector<StmtPtr> b, Span sp)
      : Stmt(StmtKind::Function, sp),
        name(std::move(n)),
        params(std::move(p)),
        body(std::move(b)) {}
};

struct ReturnStmt final : Stmt {
  ExprPtr value; // may be null for "return"
  ReturnStmt(ExprPtr v, Span sp) : Stmt(StmtKind::Return, sp), value(std::move(v)) {}
};

struct ExprStmt final : Stmt {
  ExprPtr expr;
  ExprStmt(ExprPtr e, Span sp) : Stmt(StmtKind::ExprStmt, sp), expr(std::move(e)) {}
};

// Import statement: import module, import module as alias, from module import symbol
struct ImportStmt final : Stmt {
  std::vector<std::string> modulePath;  // e.g., ["std", "math"]
  std::string alias;                    // alias name (empty if no alias)
  std::vector<std::string> symbols;     // for 'from x import a, b' (empty for 'import x')
  bool importAll = false;               // for 'from x import *'

  ImportStmt(std::vector<std::string> path, std::string aliasName, Span sp)
      : Stmt(StmtKind::Import, sp), modulePath(std::move(path)), alias(std::move(aliasName)) {}

  ImportStmt(std::vector<std::string> path, std::vector<std::string> syms, bool all, Span sp)
      : Stmt(StmtKind::Import, sp), modulePath(std::move(path)), symbols(std::move(syms)), importAll(all) {}
};

// Export statement: export { symbol1, symbol2 }
struct ExportStmt final : Stmt {
  std::vector<std::string> symbols;  // explicit exports (empty means export all public)
  ExportStmt(std::vector<std::string> syms, Span sp)
      : Stmt(StmtKind::Export, sp), symbols(std::move(syms)) {}
};

} // namespace epp

