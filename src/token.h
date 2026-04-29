#pragma once

#include "source.h"

#include <string>

namespace epp {

enum class TokenType {
  // single-char
  LBracket,   // [
  RBracket,   // ]
  LParen,     // (
  RParen,     // )
  Comma,      // ,
  Plus,       // +
  Minus,      // -
  Star,       // *
  Slash,      // /

  // literals / identifiers
  Number,
  String,
  Identifier,

  // structural
  Newline,
  Eof,

  // keywords
  Set,
  To,
  Say,
  Ask,
  Into,
  If,
  Then,
  Else,
  End,
  While,
  Do,
  Try,
  Catch,
  Function,
  Return,
  And,
  Or,
  At,
  Is,
  Not,
  Equal,
  Greater,
  Less,
  Than,
  True,
  False,
};

inline const char* tokenTypeName(TokenType t) {
  switch (t) {
    case TokenType::LBracket: return "LBracket";
    case TokenType::RBracket: return "RBracket";
    case TokenType::LParen: return "LParen";
    case TokenType::RParen: return "RParen";
    case TokenType::Comma: return "Comma";
    case TokenType::Plus: return "Plus";
    case TokenType::Minus: return "Minus";
    case TokenType::Star: return "Star";
    case TokenType::Slash: return "Slash";
    case TokenType::Number: return "Number";
    case TokenType::String: return "String";
    case TokenType::Identifier: return "Identifier";
    case TokenType::Newline: return "Newline";
    case TokenType::Eof: return "Eof";
    case TokenType::Set: return "set";
    case TokenType::To: return "to";
    case TokenType::Say: return "say";
    case TokenType::Ask: return "ask";
    case TokenType::Into: return "into";
    case TokenType::If: return "if";
    case TokenType::Then: return "then";
    case TokenType::Else: return "else";
    case TokenType::End: return "end";
    case TokenType::While: return "while";
    case TokenType::Do: return "do";
    case TokenType::Try: return "try";
    case TokenType::Catch: return "catch";
    case TokenType::Function: return "function";
    case TokenType::Return: return "return";
    case TokenType::And: return "and";
    case TokenType::Or: return "or";
    case TokenType::At: return "at";
    case TokenType::Is: return "is";
    case TokenType::Not: return "not";
    case TokenType::Equal: return "equal";
    case TokenType::Greater: return "greater";
    case TokenType::Less: return "less";
    case TokenType::Than: return "than";
    case TokenType::True: return "true";
    case TokenType::False: return "false";
  }
  return "Unknown";
}

struct Token {
  TokenType type;
  std::string lexeme;
  Span span;
};

} // namespace epp

