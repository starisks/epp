#pragma once

#include "ast.h"
#include "token.h"

#include <string>
#include <vector>

namespace epp {

struct ParseError {
  std::string message;
  Span span;
};

struct ParseResult {
  std::vector<StmtPtr> program;
  std::vector<ParseError> errors;
};

ParseResult parse(const std::vector<Token>& tokens);

} // namespace epp

