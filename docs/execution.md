# E++ Core Language Documentation

This document defines the execution model of the E++ programming language.

## Overview
E++ executes programs using a multi-stage interpreter pipeline:

1. Lexing
2. Parsing (AST generation)
3. Runtime execution

---

## Architecture

### 1. Lexer
Converts source code into tokens:
- keywords
- identifiers
- literals
- operators

Output: token stream

---

### 2. Parser (AST)
Transforms tokens into an Abstract Syntax Tree.

AST nodes include:
- variables
- functions
- expressions
- control flow

Output: AST

---

### 3. Runtime (Interpreter)
Executes AST nodes.

Handles:
- variables (symbol table)
- function calls (stack)
- expressions
- control flow

---

## Memory Model
- Symbol table (variables)
- Call stack (functions)
- Temporary evaluation stack

---

## Execution Flow

Source → Lexer → Tokens → Parser → AST → Runtime → Output

---

## Error Handling

### Compile-time
- syntax errors
- unexpected tokens

### Runtime
- undefined variables
- division by zero
- type errors
- index out of bounds

Includes:
- line number
- column number
- error message

---

## Design Goal
E++ focuses on:
- readability
- simplicity
- education
- extensibility
