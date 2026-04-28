package epp;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

public final class Lexer {
  public static final class Result {
    public final List<Token> tokens;
    public final List<LexError> errors;
    public Result(List<Token> tokens, List<LexError> errors) {
      this.tokens = tokens;
      this.errors = errors;
    }
  }

  private static final Map<String, TokenType> KEYWORDS = new HashMap<>();
  static {
    KEYWORDS.put("set", TokenType.SET);
    KEYWORDS.put("to", TokenType.TO);
    KEYWORDS.put("say", TokenType.SAY);
    KEYWORDS.put("ask", TokenType.ASK);
    KEYWORDS.put("into", TokenType.INTO);
    KEYWORDS.put("if", TokenType.IF);
    KEYWORDS.put("then", TokenType.THEN);
    KEYWORDS.put("else", TokenType.ELSE);
    KEYWORDS.put("end", TokenType.END);
    KEYWORDS.put("while", TokenType.WHILE);
    KEYWORDS.put("do", TokenType.DO);
    KEYWORDS.put("try", TokenType.TRY);
    KEYWORDS.put("catch", TokenType.CATCH);
    KEYWORDS.put("function", TokenType.FUNCTION);
    KEYWORDS.put("return", TokenType.RETURN);
    KEYWORDS.put("and", TokenType.AND);
    KEYWORDS.put("at", TokenType.AT);
    KEYWORDS.put("is", TokenType.IS);
    KEYWORDS.put("not", TokenType.NOT);
    KEYWORDS.put("equal", TokenType.EQUAL);
    KEYWORDS.put("greater", TokenType.GREATER);
    KEYWORDS.put("less", TokenType.LESS);
    KEYWORDS.put("than", TokenType.THAN);
    KEYWORDS.put("true", TokenType.TRUE);
    KEYWORDS.put("false", TokenType.FALSE);
  }

  private final String text;
  private int i = 0;
  private int line = 1;
  private int col = 1;

  private final List<Token> tokens = new ArrayList<>();
  private final List<LexError> errors = new ArrayList<>();

  private Lexer(String text) { this.text = text; }

  public static Result lex(String text) {
    return new Lexer(text).run();
  }

  private Result run() {
    while (!atEnd()) {
      char c = peek(0);
      Span sp = spanNow();

      if (c == ' ' || c == '\t' || c == '\r') { advance(); continue; }
      if (c == '#') { while (!atEnd() && peek(0) != '\n') advance(); continue; }
      if (c == '\n') { advance(); add(TokenType.NEWLINE, "\n", sp); continue; }

      switch (c) {
        case '[': advance(); add(TokenType.LBRACKET, "[", sp); continue;
        case ']': advance(); add(TokenType.RBRACKET, "]", sp); continue;
        case '(': advance(); add(TokenType.LPAREN, "(", sp); continue;
        case ')': advance(); add(TokenType.RPAREN, ")", sp); continue;
        case ',': advance(); add(TokenType.COMMA, ",", sp); continue;
        case '+': advance(); add(TokenType.PLUS, "+", sp); continue;
        case '-': advance(); add(TokenType.MINUS, "-", sp); continue;
        case '*': advance(); add(TokenType.STAR, "*", sp); continue;
        case '/': advance(); add(TokenType.SLASH, "/", sp); continue;
        case '"': readString(sp); continue;
        default: break;
      }

      if (isDigit(c)) { readNumber(sp); continue; }
      if (isIdentStart(c)) { readIdent(sp); continue; }

      errors.add(new LexError("Unexpected character: '" + c + "'", sp));
      advance();
    }

    tokens.add(new Token(TokenType.EOF, "", new Span(line, col)));
    return new Result(tokens, errors);
  }

  private void readString(Span sp) {
    advance(); // opening quote
    StringBuilder sb = new StringBuilder();
    while (!atEnd() && peek(0) != '"') {
      char ch = advance();
      if (ch == '\\' && !atEnd()) {
        char esc = advance();
        switch (esc) {
          case 'n': sb.append('\n'); break;
          case 't': sb.append('\t'); break;
          case '"': sb.append('"'); break;
          case '\\': sb.append('\\'); break;
          default: sb.append(esc); break;
        }
      } else {
        sb.append(ch);
      }
    }
    if (atEnd() || peek(0) != '"') {
      errors.add(new LexError("Unterminated string literal", sp));
      return;
    }
    advance(); // closing quote
    add(TokenType.STRING, sb.toString(), sp);
  }

  private void readNumber(Span sp) {
    StringBuilder sb = new StringBuilder();
    while (!atEnd() && isDigit(peek(0))) sb.append(advance());
    if (!atEnd() && peek(0) == '.' && !atEnd(i + 1) && isDigit(peek(1))) {
      sb.append(advance()); // '.'
      while (!atEnd() && isDigit(peek(0))) sb.append(advance());
    }
    add(TokenType.NUMBER, sb.toString(), sp);
  }

  private void readIdent(Span sp) {
    StringBuilder sb = new StringBuilder();
    while (!atEnd() && isIdentChar(peek(0))) sb.append(advance());
    String raw = sb.toString();
    String lowered = raw.toLowerCase();
    TokenType kw = KEYWORDS.get(lowered);
    if (kw != null) add(kw, lowered, sp);
    else add(TokenType.IDENT, raw, sp);
  }

  private void add(TokenType t, String lexeme, Span sp) {
    tokens.add(new Token(t, lexeme, sp));
  }

  private Span spanNow() { return new Span(line, col); }

  private boolean atEnd() { return i >= text.length(); }
  private boolean atEnd(int idx) { return idx >= text.length(); }

  private char peek(int k) {
    int idx = i + k;
    if (idx >= text.length()) return '\0';
    return text.charAt(idx);
  }

  private char advance() {
    char c = text.charAt(i++);
    if (c == '\n') { line++; col = 1; }
    else col++;
    return c;
  }

  private static boolean isDigit(char c) { return c >= '0' && c <= '9'; }
  private static boolean isIdentStart(char c) { return Character.isLetter(c) || c == '_'; }
  private static boolean isIdentChar(char c) { return Character.isLetterOrDigit(c) || c == '_'; }
}

