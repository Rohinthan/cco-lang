# Cco Compiler — Phase 10 Language Syntax Design & Ergonomics Specification

## 1. Motivation

The Cco programming language was designed as a statically typed, compiled systems language offering native x86-64 machine code generation, explicit compiler pipelines, SSA optimizations, deterministic profile-guided optimization (PGO), and single-ownership memory management without a tracing garbage collector.

Prior to Phase 10, Cco's surface syntax leaned heavily on traditional C/Rust punctuation:
- Compulsory parentheses around conditional and loop predicates (`if (cond)`, `while (cond)`).
- Low-level C-style punctuation for logical operations (`&&`, `||`, `!`).
- C-style counted loops (`for (let i: int = 0; i < n; i++)`) alongside collection iterations.
- Verbose block bodies for one-liner functions and methods (`fn f() -> int { return expr; }`).
- Compulsory C-style `//` and `/* */` comments without lightweight `#` scripting comments.

While precise, these syntactic requirements imposed visual friction and verbosity on ordinary programs, scripting tasks, and algorithms. The mission of Phase 10 is to introduce Python-inspired readability and surface ergonomics to Cco while strictly preserving:
- Concrete static compilation and compile-time type safety.
- Complete absence of runtime garbage collection or dynamic runtime interpretation.
- Deterministic IR lowering, SSA construction, register allocation, and internal ELF64 linking.
- Full 100% backward compatibility with all existing Cco syntax, tests, and self-hosted code.

---

## 2. Design Goals & Principles

The central design question is:
> *"How much can Cco syntax be simplified without weakening static semantics or introducing grammar ambiguity?"*

Phase 10 adheres to the following foundational principles:
1. **Readability with Minimal Punctuation:** Remove mandatory parentheses around conditions and provide clean keyword-based boolean operators.
2. **Predictable & Deterministic Grammar:** The grammar must remain unambiguous and LL(1) parseable with single-token lookahead (and bounded delimiter matching for conditions).
3. **Explicit Native Semantics:** No dynamic type coercion, duck typing, or implicit allocations. All expressions evaluate to static types at compile time.
4. **Canonical AST Desugaring:** Surface syntax sugar must desugar directly into existing canonical AST structures (`NODE_FOR`, `NODE_RETURN`, `NODE_BINARY`, `NODE_UNARY`) so that downstream IR lowering, optimization passes, and backend codegen remain completely untouched and robust.
5. **Clean Diagnostics:** Syntax errors must pinpoint exact source locations with context carets and actionable suggestions.
6. **Zero Memory Overhead:** No new runtime structures or dynamic metadata.
7. **Absolute Backward Compatibility:** All existing Cco programs (including all 111 integration tests and the 4-stage self-hosting bootstrap pipeline) must continue to compile and produce byte-identical results.

---

## 3. Current Syntax vs. Proposed Surface Syntax

| Feature | Legacy Cco Syntax | Phase 10 Python-Inspired Ergonomics |
| :--- | :--- | :--- |
| **Comments** | `// single-line`, `/* block */` | `# single-line` alongside `//` and `/* */` |
| **Logical AND** | `a && b` | `a and b` alongside `a && b` |
| **Logical OR** | `a \|\| b` | `a or b` alongside `a \|\| b` |
| **Logical NOT** | `!a` | `not a` alongside `!a` |
| **If Condition** | `if (x > 10) { ... }` | `if x > 10 { ... }` (parens optional) |
| **While Condition** | `while (count < 10) { ... }` | `while count < 10 { ... }` (parens optional) |
| **Counted Loops** | `for (let i: int = 0; i < 10; i++)` | `for i in 0..10 { ... }` |
| **Function Bodies** | `fn square(x: int) -> int { return x * x; }` | `fn square(x: int) -> int = x * x;` |
| **Top-Level Program**| `fn main() -> int { ... return 0; }` | Top-level statements synthesized into `main()` |
| **Variable Bindings**| `let x: int = 10;` | `let x = 10;` (type inference) and `let x: int = 10;` |

---

## 4. Formal Grammar Specification

The formal grammar for Cco Phase 10 is specified below in extended Backus-Naur form (EBNF):

```ebnf
Program         ::= ImportDecl* ( TopLevelDecl | Statement )* EOF ;

TopLevelDecl    ::= InterfaceDecl
                  | ImplDecl
                  | ClassDecl
                  | StructDecl
                  | EnumDecl
                  | FunctionDecl ;

ImportDecl      ::= "import" ( STRING_LIT | ModulePath ) ";" ;
ModulePath      ::= IDENT ( "::" IDENT )* ;

FunctionDecl    ::= "fn" ( IDENT | "operator" OperatorSymbol ) "(" ParameterList? ")" "->" Type ( Block | "=" Expression ";" ) ;
ParameterList   ::= Parameter ( "," Parameter )* ;
Parameter       ::= IDENT ":" ( "&" )? Type ;

Statement       ::= LetStmt
                  | AssignStmt
                  | CompoundAssignStmt
                  | IfStmt
                  | WhileStmt
                  | ForStmt
                  | MatchStmt
                  | ReturnStmt
                  | BreakStmt
                  | ContinueStmt
                  | PrintStmt
                  | Block
                  | ExprStmt ;

LetStmt         ::= "let" IDENT ( ":" Type )? "=" Expression ";" ;
AssignStmt      ::= ( IDENT | MemberAccess | IndexAccess ) "=" Expression ";" ;
CompoundAssignStmt ::= ( IDENT | MemberAccess | IndexAccess ) ( "+=" | "-=" | "*=" | "/=" | "%=" | "++" | "--" ) Expression? ";" ;

IfStmt          ::= "if" Condition Block ( "else" ( IfStmt | Block ) )? ;
WhileStmt       ::= "while" Condition Block ;
Condition       ::= "(" Expression ")" | Expression ;

ForStmt         ::= "for" "(" ( LetStmt | AssignStmt ) Expression ";" StepExpr? ")" Block
                  | "for" IDENT "in" Expression ".." Expression Block
                  | "for" IDENT "in" Expression Block ;

Block           ::= "{" Statement* "}" ;

ReturnStmt      ::= "return" Expression? ";" ;
BreakStmt       ::= "break" ";" ;
ContinueStmt    ::= "continue" ";" ;
PrintStmt       ::= "print" "(" Expression ")" ";" ;
ExprStmt        ::= Expression ";" ;

Expression      ::= LogicOr ;
LogicOr         ::= LogicAnd ( ( "or" | "||" ) LogicAnd )* ;
LogicAnd        ::= Equality ( ( "and" | "&&" ) Equality )* ;
Equality        ::= Comparison ( ( "==" | "!=" ) Comparison )* ;
Comparison      ::= Term ( ( "<" | "<=" | ">" | ">=" ) Term )* ;
Term            ::= Factor ( ( "+" | "-" ) Factor )* ;
Factor          ::= Unary ( ( "*" | "/" | "%" ) Unary )* ;
Unary           ::= ( "not" | "!" | "-" | "&" ) Unary | PrimaryPost ;
PrimaryPost     ::= Primary ( "(" ArgList? ")" | "[" Expression "]" | "." IDENT )* ;

Primary         ::= INT_LIT | FLOAT_LIT | STRING_LIT | FSTRING_LIT | CHAR_LIT
                  | "true" | "false"
                  | IDENT
                  | "(" Expression ")"
                  | "alloc" "(" Type "," Expression ")"
                  | "list_new" "(" Type ")"
                  | "map_new" "(" Type "," Type ")" ;
```

---

## 5. Lexical Enhancements

### 5.1 Single-Line Comment `#`
In `src/lexer.c`:
- When character `#` is encountered, the lexer enters line-comment mode, advancing until `\n` or `\0`.
- The column counter and line counter track characters appropriately.
- Legacy `//` single-line comments and `/* ... */` multi-line comments remain functional.
- In `selfhost/lexer_core.cco`, `#` is supported in the exact same manner.

### 5.2 Two-Character Range Operator `..` (`TOKEN_DOT_DOT`)
- Added `TOKEN_DOT_DOT` to `TokenType` in `src/lexer.h`.
- In `src/lexer.c`, when `c == '.'` and `source[pos + 1] == '.'`, a 2-character `TOKEN_DOT_DOT` is emitted with lexeme `".."`.
- Numeric literal parsing was updated: `isdigit(c)` previously continued while `source[pos] == '.'`. It is now constrained: if `source[pos] == '.'` and `source[pos + 1] == '.'`, number scanning immediately halts so `0..10` lexes as `TOKEN_INT_LIT("0")`, `TOKEN_DOT_DOT("..")`, `TOKEN_INT_LIT("10")`.
- Stringification returns `".."`.

### 5.3 Logical Keywords `and`, `or`, `not`
- Added `TOKEN_KW_AND`, `TOKEN_KW_OR`, `TOKEN_KW_NOT` to `TokenType`.
- In `check_keyword()`:
  - `"and"` -> `TOKEN_KW_AND`
  - `"or"` -> `TOKEN_KW_OR`
  - `"not"` -> `TOKEN_KW_NOT`
- Stringification returns `"and"`, `"or"`, `"not"`.
- Token dumper outputs `KEYWORD and`, `KEYWORD or`, `KEYWORD not`, matching `selfhost/lexer_core.cco`.

---

## 6. Parser Architecture & Changes

### 6.1 Parentheses-Free Conditionals (`if` & `while`)
In `parse_if_stmt` and `parse_while_stmt`:
- The helper `is_outer_condition_paren(Parser *p)` inspects the token stream from `p->current`:
  - If token is not `(`, returns `false`.
  - If token is `(`, traverses tokens balancing parentheses. If the matching `)` is immediately followed by `{` (`TOKEN_LBRACE`), it returns `true`.
  - If the matching `)` is followed by any other token (such as an arithmetic or boolean operator `*`, `+`, `and`, etc.), returns `false`, treating `(...)` as a subexpression.
- When `is_outer_condition_paren` is `true`, the outer `(` is consumed and `)` is required after the expression.
- When `false`, `parse_expr(p)` parses the condition directly up to `{`.
- Braces around the block remain strictly required, maintaining grammar determinism and avoiding the "dangling else" ambiguity.

### 6.2 Range Loop Desugaring (`for i in 0..10`)
In `parse_for_each_stmt`:
- `for <ident> in <expr>`:
  - Evaluates `<expr>`.
  - Checks `match(p, TOKEN_DOT_DOT)`.
  - If `TOKEN_DOT_DOT` is matched:
    - Parses `end_expr = parse_expr(p)`.
    - Parses `body = parse_block(p)`.
    - Desugars into a canonical `NODE_FOR` AST:
      1. **Init:** `let <var>: int = <start_expr>;`
      2. **Cond:** `<var> < <end_expr>`
      3. **Step:** `<var>++`
      4. **Body:** `body`
  - If `TOKEN_DOT_DOT` is not matched, it remains a collection `NODE_FOR_EACH`.
- This ensures counted range loops seamlessly leverage SSA construction, loop-invariant code motion (LICM), induction variable optimization, and register allocation.

### 6.3 Expression-Bodied Functions (`fn ... = expr;`)
In `parse_function` and `parse_class` method parsing:
- After parsing the return type, the parser checks `if (match(p, TOKEN_ASSIGN))`.
- If `=`:
  - Parses `ret_expr = parse_expr(p)`.
  - Consumes `;`.
  - Constructs a `NODE_BLOCK` containing a single `NODE_RETURN` with `value = ret_expr`.
- If not `=`:
  - Consumes `{` and parses standard block body via `parse_block(p)`.

### 6.4 Logical Keywords Parsing
- In `parse_unary`: accepts `TOKEN_NOT` (`!`) and `TOKEN_KW_NOT` (`not`), emitting `node->as.unary.op = "!"`.
- In `parse_logic_and`: accepts `TOKEN_AND` (`&&`) and `TOKEN_KW_AND` (`and`), emitting `node->as.binary.op = "&&"`.
- In `parse_logic_or`: accepts `TOKEN_OR` (`||`) and `TOKEN_KW_OR` (`or`), emitting `node->as.binary.op = "||"`.
- Operator strings in AST remain `"!"`, `"&&"`, and `"||"`, guaranteeing 100% interoperability with existing semantic analysis, SSA, IR generation, and native machine code generation.

---

## 7. Range Loop Semantics

Cco counted ranges (`for i in start..end`) are defined with strict static semantics:
1. **Half-Open Interval:** $[start, end)$ — `start` is inclusive; `end` is exclusive.
   - `0..5` yields sequence: `0, 1, 2, 3, 4`.
2. **Type Requirements:** Both `start` and `end` must evaluate to integer types (`int`).
3. **Empty Ranges:** If `start >= end`, the condition `i < end` evaluates to false immediately on entry; the loop executes 0 times.
4. **Negative Ranges:** Negative values are fully supported: `for i in -5..0` yields `-5, -4, -3, -2, -1`.
5. **Step Semantics:** Default step is `+1`.
6. **Side Effects in Range Bounds:**
   - `start` is evaluated once during loop initialization.
   - `end` is evaluated at the start of each iteration in canonical `for` loop lowering.

---

## 8. Collection Syntax Investigation & Design

Section 10 of Phase 10 requested investigating ergonomic syntax for collections:
```cco
let numbers = [1, 2, 3, 4];
let scores = {"alice": 10, "bob": 20};
```

### Analysis of Cco Runtime & Memory Model
1. **Current Array Architecture:**
   - In Cco, arrays are heap-allocated buffers created via `alloc(T, count)` (fixed) or `list_new(T)` (growable).
   - Array variables have single-ownership semantics; `src/scope_analysis.c` identifies arrays via `let_stmt.is_array` and injects deterministic cleanup (`free(arr)`) at scope exit.
2. **Challenges with First-Class Expression Literals:**
   - If `[1, 2, 3, 4]` is an arbitrary expression (e.g., `process([1, 2, 3])`), an anonymous heap allocation is generated without an associated binding. Without a scope variable to track ownership, this would leak memory unless temporary rvalue lifetime extension or automatic destruction is introduced.
   - If `[1, 2, 3, 4]` is restricted to `let` initialization:
     `let numbers = [1, 2, 3, 4];`
     It can be desugared to:
     ```cco
     let numbers: int[] = alloc(int, 4);
     numbers[0] = 1;
     numbers[1] = 2;
     numbers[2] = 3;
     numbers[3] = 4;
     ```
3. **Map Literals:**
   - Maps in Cco (`src/runtime_map.c`) use open-addressing hash tables created via `map_new(key_t, val_t)`.
   - `{"a": 1, "b": 2}` would desugar to `map_new` followed by successive `put` statements.
4. **Phase 10 Decision:**
   - Desugaring collection literals requires introducing rvalue lifetime tracking or restricting literals to `let` initializers. Per Section 10 and Section 25 ("DO NOT OVERBUILD / Do not implement new runtime collection semantics in this phase"), formal collection literal syntax is specified for Phase 11 once borrow-checker rvalue lifetimes are generalized.

---

## 9. Error Diagnostics Quality

Syntax diagnostics report precise source locations with context snippets:
- Filename, line number, column number.
- Distinct error categorization (`syntax error`, `lexer error`, `type error`).
- Visual underline/caret at the offending token.
- Descriptive error messages indicating what was expected.

Example:
```
syntax error: Expected '{' at 'x'
  --> tests/programs/error_test.cco:3:14
   2 | fn test() -> void {
   3 |     if a > 10 x = 20;
     |              ^ syntax error here
```

---

## 10. Backward Compatibility & Migration Strategy

1. **Total Dual-Syntax Compatibility:**
   - Every single legacy construct remains valid.
   - `&&` and `and` are interchangeable.
   - `||` and `or` are interchangeable.
   - `!` and `not` are interchangeable.
   - `if (c) { }` and `if c { }` are interchangeable.
   - `while (c) { }` and `while c { }` are interchangeable.
   - Legacy C-style `for (init; cond; step)` remains valid.
2. **Self-Hosting Preservation:**
   - The self-hosted compiler files (`selfhost/`) continue to parse without modification.
   - `selfhost/lexer_core.cco` is updated to understand `#`, `..`, and `and`/`or`/`not`, ensuring `tests/compare_lexers.sh` achieves 100% parity across all test files.

---

## 11. Rejected Alternatives

1. **Python Indentation-Based Parsing (Off-side Rule):**
   - *Rejected:* Indentation-sensitive parsing significantly complicates error recovery, multiline expressions, and self-hosted parser bootstrap. Requiring `{` and `}` delimiters preserves LL(1) simplicity while offering 95% of the visual cleanliness of Python.
2. **Implicit End-Point Inclusive Range (`..=`):**
   - *Deferred:* Rust uses `..=` for inclusive ranges. In Phase 10, canonical half-open `..` was chosen to align with standard iteration over $0 \le i < N$.
3. **Implicit Semicolon Insertion (ASI):**
   - *Rejected:* Semicolons remain required statement terminators. Semicolon-less grammars (like JavaScript or Go) introduce notorious edge-case ambiguities (e.g. return statements followed by newline). Explicit semicolons maintain deterministic compilation.
