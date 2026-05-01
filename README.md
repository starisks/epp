# E++ Programming Language (".epp")

E++ is a beginner-friendly interpreted language with English-like syntax and a clean interpreter architecture:

- **Lexer**: `src/lexer.*`
- **Parser (AST)**: `src/parser.*`, `src/ast.*`
- **Runtime (interpreter)**: `src/runtime.*`
- **CLI (file runner + REPL)**: `src/main.cpp`

## Build

### Caution

The `/src` may be not updated to work on Windows.
If you try to build from `/src` using CMake. It could not work on Windows although from Linux or MinGW64 will work

Using CMake:

```bash
cmake -S . -B build
cmake --build build --config Release
```

Run a file:

```bash
build/epp examples/01_hello.epp
```

Run REPL:

```bash
build/epp
```

Directly from .exe
```bash
./epp
```

.exe File Run
```bash
./epp examples/01_hello.epp
```


## Language design

### Core statements

- **Variables**
  - `set name to expression`

- **Output**
  - `say expression`

- **Input**
  - `ask expression into name`

- **If / else if / else**
  - ```
    if condition then
        statements...
    else if condition then
        statements...
    else
        statements...
    end
    ```

- **While**
  - ```
    while condition do
        statements...
    end
    ```

- **Try / catch**
  - ```
    try
        statements...
    catch
        statements...
    end
    ```

- **Functions**
  - Definition:
    - ```
      function name a and b and c
          statements...
          return expression
      end
      ```
  - Call:
    - `name expr and expr and expr`

### Expressions

- **Literals**: numbers (`10`, `3.14`), strings (`"hi"`), booleans (`true`, `false`)
- **Math**: `+ - * /` with standard precedence; parentheses supported.
- **Arrays**: `[1, 2, 3]`
- **Indexing**: `array at index`
- **Function calls**: `add 5 and 10`
- **String concatenation**: `say "hello " + name`

### Conditions (English comparisons)

Supported patterns:

- `x is greater than y`
- `x is less than y`
- `x is equal to y`
- `x is not equal to y`

Also supported:

- `x is not y` (treated as not-equal)

## Simple EBNF (practical)

```ebnf
program        := { statement ( NEWLINE { NEWLINE } | EOF ) } ;

statement      := set_stmt
               | say_stmt
               | ask_stmt
               | if_stmt
               | while_stmt
               | try_stmt
               | func_stmt
               | return_stmt
               | expr ;

set_stmt       := "set" IDENT "to" expr ;
say_stmt       := "say" expr ;
ask_stmt       := "ask" expr "into" IDENT ;

if_stmt        := "if" cond "then" NEWLINE block
                  { "else" "if" cond "then" NEWLINE block }
                  [ "else" NEWLINE block ]
                  "end" ;

while_stmt     := "while" cond "do" NEWLINE block "end" ;

try_stmt       := "try" NEWLINE block "catch" NEWLINE block "end" ;

func_stmt      := "function" IDENT { IDENT [ "and" IDENT ] } NEWLINE block "end" ;
return_stmt    := "return" [ expr ] ;

block          := { statement NEWLINE } ;

cond           := expr [ "is" [ "not" ] ( "greater" ["than"]
                                       | "less" ["than"]
                                       | "equal" ["to"] ) expr ] ;

expr           := term { ("+"|"-") term } ;
term           := unary { ("*"|"/") unary } ;
unary          := ["-"] postfix ;
postfix        := primary { "at" expr } ;
primary        := NUMBER | STRING | "true" | "false"
               | "[" [ expr { "," expr } ] "]"
               | "(" expr ")"
               | IDENT [ expr { "and" expr } ] ;
```

## Error handling

- **Lexer/Parser**: reports **line/col** with a human-readable message (e.g. unterminated string, expected token).
- **Runtime**: reports **line/col** for errors like:
  - undefined variable / function
  - division by zero
  - invalid types for operators
  - array index out of bounds

## Examples

See `examples/`.

