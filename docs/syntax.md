
# E++ Core Language Documentation

This document defines the formal syntax rules of the E++ programming language.



## Overview
E++ uses an English-like syntax designed for readability and simple parsing. Programs are interpreted line-by-line through an AST-based interpreter.


## Lexical Structure

### Identifiers
- Must start with a letter
- Can contain letters, numbers, and underscores

### Literals
- Numbers: 10, 3.14
- Strings: "hello"
- Booleans: true, false

### Keywords
- set
- to
- say
- ask
- into
- if / else if / else
- while
- do
- function
- return
- try / catch
- end


## Variables
set x to 10  
set name to "E++"

## Output
say "Hello World"



## Input
ask "Enter name" into user



## Conditions
if x is greater than y then  
    say "x is bigger"  
end

Supported comparisons:
- is greater than
- is less than
- is equal to
- is not equal to
- is not


## Loops
while x is less than 10 do  
    set x to x + 1  
end



## Functions
function add a and b  
    return a + b  
end

Call:
say add 5 and 10



## Expressions
- Arithmetic: + - * /
- Grouping: ( )
- Arrays: [1, 2, 3]
- Indexing: array at index
- Function calls: func a and b

