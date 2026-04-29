#include "parser.h"

#include <cstdlib>
#include <stdexcept>

namespace epp {

class Parser {
 public:
  explicit Parser(const std::vector<Token>& t) : tokens_(t) {}

  ParseResult run() {
    ParseResult out;
    while (!isAtEnd()) {
      if (match(TokenType::Newline)) continue;
      auto stmt = statement(out.errors);
      if (stmt) out.program.push_back(std::move(stmt));
      consumeNewlines();
    }
    return out;
  }

 private:
  const std::vector<Token>& tokens_;
  size_t i_ = 0;

  bool isAtEnd() const { return peek().type == TokenType::Eof; }
  const Token& peek() const { return tokens_[i_]; }
  const Token& previous() const { return tokens_[i_ - 1]; }

  bool check(TokenType t) const { return !isAtEnd() && peek().type == t; }

  const Token& advance() {
    if (!isAtEnd()) i_++;
    return previous();
  }

  bool match(TokenType t) {
    if (check(t)) {
      advance();
      return true;
    }
    return false;
  }

  void consumeNewlines() {
    while (match(TokenType::Newline)) {}
  }

  Token expect(TokenType t, std::vector<ParseError>& errors, const std::string& msg) {
    if (check(t)) return advance();
    std::string got = std::string(tokenTypeName(peek().type));
    if (!peek().lexeme.empty() && peek().type != TokenType::Newline) {
      got += " ('" + peek().lexeme + "')";
    }
    errors.push_back(ParseError{msg + " (expected " + std::string(tokenTypeName(t)) + ", got " + got + ")", peek().span});
    return Token{t, "", peek().span};
  }

  // ---- statements ----

  StmtPtr statement(std::vector<ParseError>& errors) {
    if (match(TokenType::Set)) return setStmt(errors, previous().span);
    if (match(TokenType::Say)) return sayStmt(errors, previous().span);
    if (match(TokenType::Ask)) return askStmt(errors, previous().span);
    if (match(TokenType::If)) return ifStmt(errors, previous().span);
    if (match(TokenType::While)) return whileStmt(errors, previous().span);
    if (match(TokenType::Try)) return tryCatchStmt(errors, previous().span);
    if (match(TokenType::Function)) return functionStmt(errors, previous().span);
    if (match(TokenType::Return)) return returnStmt(errors, previous().span);

    // expression statement (useful for calling functions without say)
    auto expr = expression(errors);
    if (!expr) return nullptr;
    return std::make_unique<ExprStmt>(std::move(expr), expr->span);
  }

  StmtPtr setStmt(std::vector<ParseError>& errors, Span sp) {
    Token nameTok = expect(TokenType::Identifier, errors, "Expected variable name after 'set'");
    expect(TokenType::To, errors, "Expected 'to' after variable name");
    auto val = expression(errors);
    if (!val) val = std::make_unique<NumberExpr>(0.0, sp);
    return std::make_unique<SetStmt>(nameTok.lexeme, std::move(val), sp);
  }

  StmtPtr sayStmt(std::vector<ParseError>& errors, Span sp) {
    auto val = expression(errors);
    if (!val) val = std::make_unique<StringExpr>("", sp);
    return std::make_unique<SayStmt>(std::move(val), sp);
  }

  StmtPtr askStmt(std::vector<ParseError>& errors, Span sp) {
    auto prompt = expression(errors);
    expect(TokenType::Into, errors, "Expected 'into' after ask prompt");
    Token nameTok = expect(TokenType::Identifier, errors, "Expected variable name after 'into'");
    if (!prompt) prompt = std::make_unique<StringExpr>("", sp);
    return std::make_unique<AskStmt>(std::move(prompt), nameTok.lexeme, sp);
  }

  std::vector<StmtPtr> blockUntil(std::vector<ParseError>& errors,
                                  const std::vector<TokenType>& stop) {
    std::vector<StmtPtr> out;
    consumeNewlines();
    while (!isAtEnd()) {
      bool shouldStop = false;
      for (auto t : stop) {
        if (check(t)) {
          shouldStop = true;
          break;
        }
      }
      if (shouldStop) break;
      auto st = statement(errors);
      if (st) out.push_back(std::move(st));
      consumeNewlines();
    }
    return out;
  }

  StmtPtr ifStmt(std::vector<ParseError>& errors, Span spIf) {
    std::vector<IfBranch> branches;
    IfBranch first;
    first.span = spIf;
    first.condition = conditionExpr(errors);
    expect(TokenType::Then, errors, "Expected 'then' after if condition");
    consumeNewlines();
    first.body = blockUntil(errors, {TokenType::Else, TokenType::End});
    branches.push_back(std::move(first));

    std::vector<StmtPtr> elseBody;

    while (match(TokenType::Else)) {
      Span spElse = previous().span;
      if (match(TokenType::If)) {
        IfBranch br;
        br.span = spElse;
        br.condition = conditionExpr(errors);
        expect(TokenType::Then, errors, "Expected 'then' after else if condition");
        consumeNewlines();
        br.body = blockUntil(errors, {TokenType::Else, TokenType::End});
        branches.push_back(std::move(br));
        continue;
      }
      // plain else
      consumeNewlines();
      elseBody = blockUntil(errors, {TokenType::End});
      break;
    }

    expect(TokenType::End, errors, "Expected 'end' to close if statement");
    return std::make_unique<IfStmt>(std::move(branches), std::move(elseBody), spIf);
  }

  StmtPtr whileStmt(std::vector<ParseError>& errors, Span sp) {
    auto cond = conditionExpr(errors);
    expect(TokenType::Do, errors, "Expected 'do' after while condition");
    consumeNewlines();
    auto body = blockUntil(errors, {TokenType::End});
    expect(TokenType::End, errors, "Expected 'end' to close while loop");
    if (!cond) cond = std::make_unique<BoolExpr>(false, sp);
    return std::make_unique<WhileStmt>(std::move(cond), std::move(body), sp);
  }

  StmtPtr tryCatchStmt(std::vector<ParseError>& errors, Span sp) {
    consumeNewlines();
    auto tryBody = blockUntil(errors, {TokenType::Catch, TokenType::End});
    std::vector<StmtPtr> catchBody;
    if (match(TokenType::Catch)) {
      consumeNewlines();
      catchBody = blockUntil(errors, {TokenType::End});
    } else {
      errors.push_back(ParseError{"Expected 'catch' in try/catch", peek().span});
    }
    expect(TokenType::End, errors, "Expected 'end' to close try/catch");
    return std::make_unique<TryCatchStmt>(std::move(tryBody), std::move(catchBody), sp);
  }

  StmtPtr functionStmt(std::vector<ParseError>& errors, Span sp) {
    Token nameTok = expect(TokenType::Identifier, errors, "Expected function name after 'function'");
    std::vector<std::string> params;
    // parameters: <id> (and <id>)*
    if (check(TokenType::Identifier)) {
      params.push_back(advance().lexeme);
      while (match(TokenType::And)) {
        Token p = expect(TokenType::Identifier, errors, "Expected parameter name after 'and'");
        params.push_back(p.lexeme);
      }
    }
    consumeNewlines();
    auto body = blockUntil(errors, {TokenType::End});
    expect(TokenType::End, errors, "Expected 'end' to close function");
    return std::make_unique<FunctionStmt>(nameTok.lexeme, std::move(params), std::move(body), sp);
  }

  StmtPtr returnStmt(std::vector<ParseError>& errors, Span sp) {
    // "return" or "return <expr>"
    if (check(TokenType::Newline) || check(TokenType::End) || check(TokenType::Else) ||
        check(TokenType::Catch) || check(TokenType::Eof)) {
      return std::make_unique<ReturnStmt>(nullptr, sp);
    }
    auto v = expression(errors);
    return std::make_unique<ReturnStmt>(std::move(v), sp);
  }

  // ---- expressions ----

  static bool startsExpr(TokenType t) {
    switch (t) {
      case TokenType::Number:
      case TokenType::String:
      case TokenType::Identifier:
      case TokenType::LParen:
      case TokenType::LBracket:
      case TokenType::True:
      case TokenType::False:
      case TokenType::Minus:
      case TokenType::Not:
        return true;
      default:
        return false;
    }
  }

  ExprPtr conditionExpr(std::vector<ParseError>& errors) { return logicOr(errors); }

  ExprPtr logicOr(std::vector<ParseError>& errors) {
    auto expr = logicAnd(errors);
    while (match(TokenType::Or)) {
      Span sp = previous().span;
      auto rhs = logicAnd(errors);
      if (!rhs) rhs = std::make_unique<BoolExpr>(false, sp);
      expr = std::make_unique<BinaryExpr>(BinaryOp::LogicalOr, std::move(expr), std::move(rhs), sp);
    }
    return expr;
  }

  ExprPtr logicAnd(std::vector<ParseError>& errors) {
    auto expr = logicNot(errors);
    while (match(TokenType::And)) {
      Span sp = previous().span;
      auto rhs = logicNot(errors);
      if (!rhs) rhs = std::make_unique<BoolExpr>(false, sp);
      expr = std::make_unique<BinaryExpr>(BinaryOp::LogicalAnd, std::move(expr), std::move(rhs), sp);
    }
    return expr;
  }

  ExprPtr logicNot(std::vector<ParseError>& errors) {
    if (match(TokenType::Not)) {
      Span sp = previous().span;
      auto rhs = logicNot(errors);
      if (!rhs) rhs = std::make_unique<BoolExpr>(false, sp);
      return std::make_unique<UnaryExpr>(UnaryOp::Not, std::move(rhs), sp);
    }
    return conditionAtom(errors);
  }

  ExprPtr conditionAtom(std::vector<ParseError>& errors) {
    // Allow parenthesized conditions inside if/while, e.g.:
    //   not (x is equal to 0)
    // without changing the arithmetic expression grammar used for function-call arguments.
    if (match(TokenType::LParen)) {
      Span sp = previous().span;
      auto inner = conditionExpr(errors);
      expect(TokenType::RParen, errors, "Expected ')' to close condition group");
      if (!inner) inner = std::make_unique<BoolExpr>(false, sp);
      return inner;
    }
    return comparison(errors);
  }

  ExprPtr comparison(std::vector<ParseError>& errors) {
    // English comparisons:
    // <expr> is (not)? (greater|less|equal) (than)? (to)? <expr>
    //
    // Supports chaining by rewriting:
    //   a is less than b is less than c
    // into:
    //   (a < b) and (b < c)
    auto left = expression(errors);
    if (!left) return nullptr;

    if (!check(TokenType::Is)) return left;

    ExprPtr result = nullptr;

    while (match(TokenType::Is)) {
      Span spIs = previous().span;
      bool isNot = match(TokenType::Not);

      BinaryOp op;
      bool recognized = false;

      if (match(TokenType::Greater)) {
        recognized = true;
        match(TokenType::Than);
        op = isNot ? BinaryOp::Lte : BinaryOp::Gt;
      } else if (match(TokenType::Less)) {
        recognized = true;
        match(TokenType::Than);
        op = isNot ? BinaryOp::Gte : BinaryOp::Lt;
      } else if (match(TokenType::Equal)) {
        recognized = true;
        match(TokenType::To); // optional
        op = isNot ? BinaryOp::Neq : BinaryOp::Eq;
      } else if (isNot) {
        // "is not <expr>" means !=
        recognized = true;
        op = BinaryOp::Neq;
      }

      if (!recognized) {
        errors.push_back(ParseError{"Expected comparison after 'is'", peek().span});
        return left;
      }

      auto right = expression(errors);
      if (!right) right = std::make_unique<NumberExpr>(0.0, peek().span);

      ExprPtr cmp = std::make_unique<BinaryExpr>(op, std::move(left), cloneExprForChain(*right), spIs);

      if (!result) {
        result = std::move(cmp);
      } else {
        result = std::make_unique<BinaryExpr>(BinaryOp::LogicalAnd, std::move(result), std::move(cmp), spIs);
      }

      left = std::move(right);

      // allow: "... is equal to ..." then another "is ..." immediately
      if (!check(TokenType::Is)) break;
    }

    return result ? std::move(result) : std::move(left);
  }

  static ExprPtr cloneExprForChain(const Expr& e) {
    // Minimal clone used only for comparison chaining (so the shared "right" expression can be reused).
    // For other cases we keep ownership as-is.
    switch (e.kind) {
      case ExprKind::Number: return std::make_unique<NumberExpr>(static_cast<const NumberExpr&>(e).value, e.span);
      case ExprKind::String: return std::make_unique<StringExpr>(static_cast<const StringExpr&>(e).value, e.span);
      case ExprKind::Bool: return std::make_unique<BoolExpr>(static_cast<const BoolExpr&>(e).value, e.span);
      case ExprKind::Var: return std::make_unique<VarExpr>(static_cast<const VarExpr&>(e).name, e.span);
      case ExprKind::Array: {
        const auto& ae = static_cast<const ArrayExpr&>(e);
        std::vector<ExprPtr> elems;
        elems.reserve(ae.elements.size());
        for (const auto& el : ae.elements) elems.push_back(cloneExprForChain(*el));
        return std::make_unique<ArrayExpr>(std::move(elems), e.span);
      }
      case ExprKind::Binary: {
        const auto& be = static_cast<const BinaryExpr&>(e);
        return std::make_unique<BinaryExpr>(be.op, cloneExprForChain(*be.left), cloneExprForChain(*be.right), e.span);
      }
      case ExprKind::Unary: {
        const auto& ue = static_cast<const UnaryExpr&>(e);
        return std::make_unique<UnaryExpr>(ue.op, cloneExprForChain(*ue.expr), e.span);
      }
      case ExprKind::Call: {
        const auto& ce = static_cast<const CallExpr&>(e);
        std::vector<ExprPtr> args;
        args.reserve(ce.args.size());
        for (const auto& a : ce.args) args.push_back(cloneExprForChain(*a));
        return std::make_unique<CallExpr>(ce.callee, std::move(args), e.span);
      }
      case ExprKind::Index: {
        const auto& ie = static_cast<const IndexExpr&>(e);
        return std::make_unique<IndexExpr>(cloneExprForChain(*ie.target), cloneExprForChain(*ie.index), e.span);
      }
      case ExprKind::Group: {
        const auto& ge = static_cast<const GroupExpr&>(e);
        return std::make_unique<GroupExpr>(cloneExprForChain(*ge.inner), e.span);
      }
    }
    return nullptr;
  }

  ExprPtr expression(std::vector<ParseError>& errors) { return addition(errors); }

  ExprPtr addition(std::vector<ParseError>& errors) {
    auto expr = multiplication(errors);
    while (true) {
      if (match(TokenType::Plus)) {
        Span sp = previous().span;
        auto rhs = multiplication(errors);
        if (!rhs) rhs = std::make_unique<NumberExpr>(0.0, sp);
        expr = std::make_unique<BinaryExpr>(BinaryOp::Add, std::move(expr), std::move(rhs), sp);
        continue;
      }
      if (match(TokenType::Minus)) {
        Span sp = previous().span;
        auto rhs = multiplication(errors);
        if (!rhs) rhs = std::make_unique<NumberExpr>(0.0, sp);
        expr = std::make_unique<BinaryExpr>(BinaryOp::Sub, std::move(expr), std::move(rhs), sp);
        continue;
      }
      break;
    }
    return expr;
  }

  ExprPtr multiplication(std::vector<ParseError>& errors) {
    auto expr = unary(errors);
    while (true) {
      if (match(TokenType::Star)) {
        Span sp = previous().span;
        auto rhs = unary(errors);
        if (!rhs) rhs = std::make_unique<NumberExpr>(0.0, sp);
        expr = std::make_unique<BinaryExpr>(BinaryOp::Mul, std::move(expr), std::move(rhs), sp);
        continue;
      }
      if (match(TokenType::Slash)) {
        Span sp = previous().span;
        auto rhs = unary(errors);
        if (!rhs) rhs = std::make_unique<NumberExpr>(0.0, sp);
        expr = std::make_unique<BinaryExpr>(BinaryOp::Div, std::move(expr), std::move(rhs), sp);
        continue;
      }
      break;
    }
    return expr;
  }

  ExprPtr unary(std::vector<ParseError>& errors) {
    if (match(TokenType::Minus)) {
      Span sp = previous().span;
      auto rhs = unary(errors);
      if (!rhs) rhs = std::make_unique<NumberExpr>(0.0, sp);
      return std::make_unique<UnaryExpr>(UnaryOp::Neg, std::move(rhs), sp);
    }
    if (match(TokenType::Not)) {
      Span sp = previous().span;
      auto rhs = unary(errors);
      if (!rhs) rhs = std::make_unique<BoolExpr>(false, sp);
      return std::make_unique<UnaryExpr>(UnaryOp::Not, std::move(rhs), sp);
    }
    return postfix(errors);
  }

  ExprPtr postfix(std::vector<ParseError>& errors) {
    auto expr = primary(errors);
    while (expr && match(TokenType::At)) {
      Span sp = previous().span;
      auto idx = expression(errors);
      if (!idx) idx = std::make_unique<NumberExpr>(0.0, sp);
      expr = std::make_unique<IndexExpr>(std::move(expr), std::move(idx), sp);
    }
    return expr;
  }

  ExprPtr primary(std::vector<ParseError>& errors) {
    if (match(TokenType::Number)) {
      const auto& t = previous();
      char* end = nullptr;
      double v = std::strtod(t.lexeme.c_str(), &end);
      (void)end;
      return std::make_unique<NumberExpr>(v, t.span);
    }
    if (match(TokenType::String)) {
      const auto& t = previous();
      return std::make_unique<StringExpr>(t.lexeme, t.span);
    }
    if (match(TokenType::True)) return std::make_unique<BoolExpr>(true, previous().span);
    if (match(TokenType::False)) return std::make_unique<BoolExpr>(false, previous().span);

    if (match(TokenType::LBracket)) {
      Span sp = previous().span;
      std::vector<ExprPtr> elems;
      if (!check(TokenType::RBracket)) {
        do {
          auto e = expression(errors);
          if (e) elems.push_back(std::move(e));
        } while (match(TokenType::Comma));
      }
      expect(TokenType::RBracket, errors, "Expected ']' to close array literal");
      return std::make_unique<ArrayExpr>(std::move(elems), sp);
    }

    if (match(TokenType::LParen)) {
      Span sp = previous().span;
      auto inner = expression(errors);
      expect(TokenType::RParen, errors, "Expected ')' to close group");
      if (!inner) inner = std::make_unique<NumberExpr>(0.0, sp);
      return std::make_unique<GroupExpr>(std::move(inner), sp);
    }

    if (match(TokenType::Identifier)) {
      Token id = previous();
      // call syntax: <name> <expr> (and <expr>)*
      if (startsExpr(peek().type)) {
        std::vector<ExprPtr> args;
        auto first = expression(errors);
        if (first) args.push_back(std::move(first));
        while (match(TokenType::And)) {
          auto a = expression(errors);
          if (a) args.push_back(std::move(a));
        }
        return std::make_unique<CallExpr>(id.lexeme, std::move(args), id.span);
      }
      return std::make_unique<VarExpr>(id.lexeme, id.span);
    }

    errors.push_back(ParseError{"Expected expression", peek().span});
    advance();
    return nullptr;
  }
};

ParseResult parse(const std::vector<Token>& tokens) {
  Parser p(tokens);
  return p.run();
}

} // namespace epp

