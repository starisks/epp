#pragma once

#include "source.h"
#include "token.h"

#include <string>
#include <vector>

namespace epp {

struct LexError {
  std::string message;
  Span span;
};

struct LexResult {
  std::vector<Token> tokens;
  std::vector<LexError> errors;
};

LexResult lex(const Source& src);

} // namespace epp

