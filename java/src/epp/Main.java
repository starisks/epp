package epp;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.List;

public final class Main {
  private static void printLexErrors(List<LexError> errs) {
    for (var e : errs) {
      System.err.println("Lexer error at line " + e.span.line + ", col " + e.span.col + ": " + e.message);
    }
  }

  private static void printParseErrors(List<ParseError> errs) {
    for (var e : errs) {
      System.err.println("Parser error at line " + e.span.line + ", col " + e.span.col + ": " + e.message);
    }
  }

  private static void printRuntimeErrors(List<RuntimeError> errs) {
    for (var e : errs) {
      System.err.println("Runtime error at line " + e.span.line + ", col " + e.span.col + ": " + e.message);
    }
  }

  private static int runText(String text, boolean replMode) {
    var lr = Lexer.lex(text);
    if (!lr.errors.isEmpty()) {
      printLexErrors(lr.errors);
      return 1;
    }

    var pr = Parser.parse(lr.tokens);
    if (!pr.errors.isEmpty()) {
      printParseErrors(pr.errors);
      return 1;
    }

    var it = new Interpreter();
    var er = it.exec(pr.program);
    if (!er.ok) {
      printRuntimeErrors(er.errors);
      return replMode ? 0 : 1;
    }
    return 0;
  }

  private static int repl() throws IOException {
    System.out.println("E++ REPL. Type 'exit' to quit.");
    var in = new java.io.BufferedReader(new java.io.InputStreamReader(System.in));
    String buffer = "";
    int depth = 0;

    while (true) {
      System.out.print(depth > 0 ? "... " : ">>> ");
      String line = in.readLine();
      if (line == null) break;
      if (depth == 0 && line.trim().equals("exit")) break;

      depth = updateDepth(depth, line);
      buffer += line + "\n";

      if (depth == 0) {
        runText(buffer, true);
        buffer = "";
      }
    }
    return 0;
  }

  private static int updateDepth(int depth, String line) {
    String[] parts = line.trim().split("\\s+");
    if (parts.length == 0) return depth;
    String w = parts[0].toLowerCase();
    if (w.equals("if") || w.equals("while") || w.equals("try") || w.equals("function")) depth++;
    if (w.equals("end")) depth = Math.max(0, depth - 1);
    return depth;
  }

  public static void main(String[] args) throws Exception {
    if (args.length == 0) {
      System.exit(repl());
      return;
    }

    String path = args[0];
    String text;
    try {
      text = Files.readString(Path.of(path));
    } catch (IOException e) {
      System.err.println("Could not read file: " + path);
      System.exit(1);
      return;
    }

    System.exit(runText(text, false));
  }
}

