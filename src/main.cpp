#include "lexer.h"
#include "parser.h"
#include "runtime.h"
#include "source.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <curl/curl.h>

#define EPP_VERSION "v0.2.0"
#define API_URL "https://api.github.com/repos/starisks/epp/releases/latest"

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
  for (const auto& e : re) printError(e);
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

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* output) {
  output->append((char*)contents, size * nmemb);
  return size * nmemb;
}

static std::string fetchLatestVersion() {
  CURL* curl = curl_easy_init();
  if (!curl) return "";

  std::string buffer;

  curl_easy_setopt(curl, CURLOPT_URL, API_URL);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "epp-cli"); // required

  CURLcode res = curl_easy_perform(curl);
  curl_easy_cleanup(curl);

  if (res != CURLE_OK) return "";

  // extract "tag_name"
  size_t pos = buffer.find("\"tag_name\"");
  if (pos == std::string::npos) return "";

  size_t start = buffer.find("\"", pos + 10);
  size_t end = buffer.find("\"", start + 1);

  if (start == std::string::npos || end == std::string::npos) return "";

  return buffer.substr(start + 1, end - start - 1);
}

static std::vector<int> parseVersion(std::string v) {
  if (!v.empty() && v[0] == 'v') v = v.substr(1);

  std::vector<int> parts;
  std::stringstream ss(v);
  std::string item;

  while (std::getline(ss, item, '.')) {
    parts.push_back(std::stoi(item));
  }
  return parts;
}

static bool isNewer(std::string latest, std::string current) {
  auto l = parseVersion(latest);
  auto c = parseVersion(current);

  size_t n = std::max(l.size(), c.size());

  for (size_t i = 0; i < n; i++) {
    int lv = (i < l.size()) ? l[i] : 0;
    int cv = (i < c.size()) ? c[i] : 0;

    if (lv > cv) return true;
    if (lv < cv) return false;
  }
  return false;
}

static void showVersion() {
  std::cout << "E++ " << EPP_VERSION << "\n";

  std::string latest = fetchLatestVersion();

  if (latest.empty()) {
    std::cout << "(could not check for updates)\n";
    return;
  }

  if (isNewer(latest, EPP_VERSION)) {
    std::cout << "Update available: " << latest << "\n";
  } else {
    std::cout << "(up to date)\n";
  }
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

  // 1. Handle CLI flags FIRST
  if (argc > 1) {
    std::string arg = argv[1];

  if (arg == "--version") {
    showVersion();
    return 0;
  } 

  if (arg == "--help") {
    std::cout <<
      "E++ CLI (epp)\n\n"
      "Usage:\n"
      "  epp                Start REPL\n"
      "  epp <file>.epp     Run a file\n\n"
      "Options:\n"
      "  --version          Show version and check for updates\n"
      "  --help             Show this help message\n";
    return 0;
    } 
  }

  // 2. REPL mode
  if (argc <= 1) {
    return repl();
  }

  // 3. File execution
  std::string path = argv[1];
  std::string text = readFileText(path);

  if (text.empty()) {
    std::cerr << "Could not read file: " << path << "\n";
    return 1;
  }

  return runText(text, path, false);
}

