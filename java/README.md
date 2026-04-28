# E++ (Java) — runnable interpreter

This folder contains a complete Java interpreter for E++ with the required features:

- variables (dynamic typing)
- say / ask
- math expressions + nesting
- if / else if / else
- while
- arrays + `at` indexing
- try / catch
- functions + `return`
- file execution + REPL

## Run (Windows PowerShell)

From repo root:

```powershell
mkdir -Force java\out | Out-Null
javac -d java\out (Get-ChildItem -Recurse java\src -Filter *.java | % FullName)
java -cp java\out epp.Main examples\01_hello.epp
```

REPL:

```powershell
java -cp java\out epp.Main
```

