#include "lexer.h"
#include "parser.h"
#include "runtime.h"
#include "source.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace epp {

static std::string readFileText(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) return "";
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

static void printErrors(const std::string& stage, const std::vector<LexError>& le,
                        const std::vector<ParseError>& pe, const std::vector<RuntimeError>& re) {
  auto printOne = [&](const std::string& kind, const std::string& msg, Span sp) {
    std::cerr << stage << " " << kind << " at line " << sp.line << ", col " << sp.col << ": " << msg
              << "\n";
  };

  for (const auto& e : le) printOne("error", e.message, e.span);
  for (const auto& e : pe) printOne("error", e.message, e.span);
  for (const auto& e : re) printOne("error", e.message, e.span);
}

static int runText(const std::string& text, const std::string& path, bool replMode) {
  Source src{path, text};

  auto lr = lex(src);
  if (!lr.errors.empty()) {
    printErrors("Lexer", lr.errors, {}, {});
    return 1;
  }

  auto pr = parse(lr.tokens);
  if (!pr.errors.empty()) {
    printErrors("Parser", {}, pr.errors, {});
    return 1;
  }

  auto er = runProgram(pr.program, std::cin, std::cout);
  if (!er.ok) {
    printErrors("Runtime", {}, {}, er.errors);
    return replMode ? 0 : 1;
  }
  return 0;
}

static int repl() {
  std::cout << "E++ REPL. Type 'exit' to quit.\n";
  std::string line;
  std::string buffer;
  int depth = 0;

  auto updateDepth = [&](const std::string& ln) {
    // Simple depth tracker for blocks: increase on leading keywords, decrease on "end".
    // This isn't a full parser; it's just to decide when to run the buffer.
    std::istringstream ss(ln);
    std::string w1, w2;
    ss >> w1;
    std::string lw1;
    for (char c : w1) lw1.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    if (lw1 == "if" || lw1 == "while" || lw1 == "try" || lw1 == "function") depth++;
    if (lw1 == "end") depth = std::max(0, depth - 1);
    // "else if" doesn't change depth; "else" doesn't change depth.
  };

  while (true) {
    std::cout << (depth > 0 ? "... " : ">>> ");
    if (!std::getline(std::cin, line)) break;
    if (depth == 0) {
      std::string trimmed = line;
      while (!trimmed.empty() && (trimmed.back() == ' ' || trimmed.back() == '\t')) trimmed.pop_back();
      if (trimmed == "exit") break;
    }

    updateDepth(line);
    buffer += line;
    buffer += "\n";

    if (depth == 0) {
      (void)runText(buffer, "<repl>", true);
      buffer.clear();
    }
  }
  return 0;
}

} // namespace epp

int main(int argc, char** argv) {
  using namespace epp;

  if (argc <= 1) {
    return repl();
  }

  std::string path = argv[1];
  std::string text = readFileText(path);
  if (text.empty()) {
    std::cerr << "Could not read file: " << path << "\n";
    return 1;
  }
  return runText(text, path, false);
}

