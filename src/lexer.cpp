#include "lexer.h"

#include <cctype>
#include <unordered_map>

namespace epp {

static bool isIdentStart(char c) {
  return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
}

static bool isIdentChar(char c) {
  return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

static std::string lowerAscii(std::string s) {
  for (auto& ch : s) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  return s;
}

LexResult lex(const Source& src) {
  static const std::unordered_map<std::string, TokenType> kKeywords = {
    {"set", TokenType::Set},
    {"to", TokenType::To},
    {"say", TokenType::Say},
    {"ask", TokenType::Ask},
    {"into", TokenType::Into},
    {"if", TokenType::If},
    {"then", TokenType::Then},
    {"else", TokenType::Else},
    {"end", TokenType::End},
    {"while", TokenType::While},
    {"do", TokenType::Do},
    {"try", TokenType::Try},
    {"catch", TokenType::Catch},
    {"function", TokenType::Function},
    {"return", TokenType::Return},
    {"and", TokenType::And},
    {"or", TokenType::Or},
    {"at", TokenType::At},
    {"is", TokenType::Is},
    {"not", TokenType::Not},
    {"equal", TokenType::Equal},
    {"greater", TokenType::Greater},
    {"less", TokenType::Less},
    {"than", TokenType::Than},
    {"true", TokenType::True},
    {"false", TokenType::False},
  };

  LexResult out;
  const std::string& s = src.text;
  size_t i = 0;
  int line = 1;
  int col = 1;

  auto spanNow = [&]() -> Span { return Span{line, col}; };

  auto advance = [&]() -> char {
    char c = s[i++];
    if (c == '\n') {
      line++;
      col = 1;
    } else {
      col++;
    }
    return c;
  };

  auto peek = [&](size_t k = 0) -> char {
    if (i + k >= s.size()) return '\0';
    return s[i + k];
  };

  auto add = [&](TokenType type, std::string lexeme, Span sp) {
    out.tokens.push_back(Token{type, std::move(lexeme), sp});
  };

  while (i < s.size()) {
    char c = peek();
    Span sp = spanNow();

    if (c == ' ' || c == '\t' || c == '\r') {
      advance();
      continue;
    }

    if (c == '#') {
      while (i < s.size() && peek() != '\n') advance();
      continue;
    }

    if (c == '/' && peek(1) == '/') {
      while (i < s.size() && peek() != '\n') advance();
      continue;
    }

    if (c == '\n') {
      advance();
      add(TokenType::Newline, "\n", sp);
      continue;
    }

    switch (c) {
      case '[': advance(); add(TokenType::LBracket, "[", sp); continue;
      case ']': advance(); add(TokenType::RBracket, "]", sp); continue;
      case '(': advance(); add(TokenType::LParen, "(", sp); continue;
      case ')': advance(); add(TokenType::RParen, ")", sp); continue;
      case ',': advance(); add(TokenType::Comma, ",", sp); continue;
      case '+': advance(); add(TokenType::Plus, "+", sp); continue;
      case '-': advance(); add(TokenType::Minus, "-", sp); continue;
      case '*': advance(); add(TokenType::Star, "*", sp); continue;
      case '/': advance(); add(TokenType::Slash, "/", sp); continue;
      case '"': {
        advance(); // opening quote
        std::string str;
        while (i < s.size() && peek() != '"') {
          char ch = advance();
          if (ch == '\\' && i < s.size()) {
            char esc = advance();
            switch (esc) {
              case 'n': str.push_back('\n'); break;
              case 't': str.push_back('\t'); break;
              case '"': str.push_back('"'); break;
              case '\\': str.push_back('\\'); break;
              default: str.push_back(esc); break;
            }
          } else {
            str.push_back(ch);
          }
        }
        if (peek() != '"') {
          out.errors.push_back(LexError{"Unterminated string literal", sp});
          break;
        }
        advance(); // closing quote
        add(TokenType::String, str, sp);
        continue;
      }
      default: break;
    }

    if (std::isdigit(static_cast<unsigned char>(c))) {
      std::string num;
      while (std::isdigit(static_cast<unsigned char>(peek()))) num.push_back(advance());
      if (peek() == '.' && std::isdigit(static_cast<unsigned char>(peek(1)))) {
        num.push_back(advance());
        while (std::isdigit(static_cast<unsigned char>(peek()))) num.push_back(advance());
      }
      add(TokenType::Number, num, sp);
      continue;
    }

    if (isIdentStart(c)) {
      std::string id;
      while (isIdentChar(peek())) id.push_back(advance());
      std::string lowered = lowerAscii(id);
      auto it = kKeywords.find(lowered);
      if (it != kKeywords.end()) {
        add(it->second, lowered, sp);
      } else {
        add(TokenType::Identifier, id, sp);
      }
      continue;
    }

    out.errors.push_back(LexError{std::string("Unexpected character: '") + c + "'", sp});
    advance();
  }

  out.tokens.push_back(Token{TokenType::Eof, "", Span{line, col}});
  return out;
}

} // namespace epp

