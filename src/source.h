#pragma once

#include <string>

namespace epp {

struct Source {
  std::string path;
  std::string text;
};

struct Span {
  int line = 1;
  int col = 1;
};

} // namespace epp

