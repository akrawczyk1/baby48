# baby48

C++ emulator and assembler for the Manchester Small-Scale Experimental Machine
(SSEM, "the Baby"), the first electronic stored-program computer. Its first
program ran on 21 June 1948.

Personal project. Goals: finish a complete, well-structured project, and
relearn modern C++ (value types, RAII, `std::array`, `enum class`,
`std::string_view`, `std::expected`; no owning raw pointers).

## v1 scope

- Faithful Baby core (32-word store)
- Assembler (line-numbered syntax, below)
- CLI: run / step / breakpoints / instruction limit
- Text memory dump in the Baby's dot/dash monitor format
- Kilburn's program test cases passing (see Tests)

See roadmap for post-v1 ideas.

## Architecture

Dependencies point one way: `isa`, `memory` <- `machine`, `assembler`,
`format` <- `cli`. `isa` and `memory` are independent of each other.

- `isa`: the encoding spec, standing in for the hardware decoder. Opcode enum,
  bit-field masks, `encode`/`decode`, mnemonic table, whether an op uses its
  operand, and how many lines an instruction can address (`lineCount`, from the
  5-bit S field). No execution semantics.
- `memory`: the shape of the store: `storeSize` and `MemoryImage`
  (`std::array<std::uint32_t, storeSize>`). No behaviour. Separate from `isa`
  so that the store size can vary independently of the instruction format (see
  Roadmap).
- `machine`: machine state + `step()`, read-only accessors. All execution
  semantics live here. No I/O. Constructed from a `MemoryImage` with registers
  reset. `step()` returns a status (ok / halted / fault). Combines `isa` and
  `memory`: CI and operands select store lines by line number, so the faithful
  machine requires `memory::storeSize == isa::lineCount`, checked at compile
  time.
- `assembler`: source text -> `std::expected<MemoryImage, AsmError>`. Knows
  nothing about Machine.
- `format`: pure functions, machine state -> string. Never prints.
- `cli`: the only module that touches files, arguments, or stdout. Owns the run
  loop: until halted, until a breakpoint (check CI before each step), or until an
  instruction limit.

## Machine spec (from Burton's Programmer's Reference Manual)

- Store: 32 lines x 32 bits. Accumulator A: 32 bits. CI (control instruction,
  i.e. program counter): a full 32-bit word.
- Numbers: 32-bit two's complement, no overflow detection, carries lost. Use
  `uint32_t` wraparound arithmetic; convert to signed only to interpret.
- Instruction word: line number S = bits 0-4 (`word & 0x1F`); function number
  F = bits 13-15 (`(word >> 13) & 0x7`). All other bits ignored by the decoder.
- Bit 0 is the LSB. Historical documents WRITE binary LSB-first (left to right),
  so use the function numbers, not the printed binary strings.
- Cycle: CI <- CI + 1; PI <- store[CI & 0x1F]; decode; execute. The first
  instruction executed is therefore line 1.

| F | Mnemonic | Effect |
|---|----------|--------|
| 0 | JMP S | CI <- store[S] (indirect; store target - 1) |
| 1 | JRP S | CI <- CI + store[S] |
| 2 | LDN S | A <- -store[S] |
| 3 | STO S | store[S] <- A |
| 4 | SUB S | A <- A - store[S] |
| 5 | SUB S | same as 4 |
| 6 | CMP | if A < 0, skip next instruction |
| 7 | STOP | halt |

## Fidelity decisions

- CMP is implemented as an immediate CI += 1 (observably identical to the
  hardware's test flip-flop).
- Function 5 decodes as SUB.
- CI bits 13-15 becoming nonzero (which hung the real machine) is detected and
  reported as a fault, not emulated. The check happens immediately after the
  CI increment, before the fetch. A faulting step executes nothing and is not
  counted. Checking at this point catches both a jump and an increment past
  `0x1FFF`, and allows a jump to line 0 through a stored -1 (`0xFFFFFFFF` wraps
  to 0 on the increment). The fetch is routed through the function decoder, so
  nonzero bits 13-15 leave the instruction with no path into PI (Burton,
  Appendix A1.4). One case did not hang: function bits of 1 added the fetched
  word to the PI left over from the previous instruction. The emulator reports
  this case as a fault too, rather than reproducing a corrupted instruction.
- `step()` on a halted machine returns halted again and changes nothing: no CI
  increment, no instruction counted. A faulted machine likewise keeps returning
  fault. Run loops call `step()` until it stops returning ok.
- The instruction count includes the STOP instruction.

## Assembler syntax (v1)

- One statement per line: `<line>: <MNEMONIC> [operand]`, e.g. `1: LDN 18`
- Data: `<line>: NUM <value>` (signed 32-bit)
- `CMP` / `STOP` accept and ignore an operand (historical listings write `CMP 0`)
- `STOP` opcode defined as four letter version for easier eyeballing of program halt location
- `STP` accepted as historical spelling of STOP
- Opcode matching is case-insensitive
- Errors, each reported with its source line number: unknown mnemonic; missing
  or extra operand; operand or line number outside 0-31; duplicate line
  numbers; NUM value outside 32-bit range
- OPEN: comment syntax. Labels are deferred to v1.1 (they need a second pass).

## Memory dump format

One row per store line: line number, 32 characters with `.` = 0 and `-` = 1,
LSB first (matching the Baby's monitor tube), disassembly, decimal value. Then A
and CI rows in the same style. Example:

```
  1: .-..-.........-................. LDN 18   ;     16402
```

## Tests

- `encode(LDN, 18) == 16402`; encode/decode round-trips.
- Machine: each instruction in isolation using hand-encoded memory images.
  JMP, JRP and CMP need the most coverage (indirection, CI pre-increment).
- Kilburn's highest-factor program (Kilburn/Tootill 1996 reconstruction of the
  21 June 1948 version), listed below. The answer ends up in line 20 as
  (answer - 1), because b is decremented before the remainder test.
  - a = 989 (23 x 43): line 20 = 42, exactly 21,387 instructions executed
    counting STOP (cross-checked against Mark Stevens's emulator)
  - a = 3141 (3 x 1047): line 20 = 1046 (from the 1948 operating notes)
  - a = 4537 (13 x 349): line 20 = 348 (from the 1948 operating notes)
  - a = 2^18: line 20 = 131071, 2,228,228 instructions (computed by hand,
    not yet cross-checked)
- Further fixtures: HASE's 18 July 1948 amended version and its
  every-instruction demo program.

Kilburn's program, data shown for a = 989. For another a: line 19 = -a,
line 20 = a - 1, line 22 = -(a - 1).

```
 1: LDN 18      ; A = 0 (clear)
 2: LDN 19      ; A = a
 3: SUB 20      ; A -= b  (inner loop)
 4: CMP
 5: JRP 21      ; back to line 3
 6: SUB 22      ; A += b -> remainder r
 7: STO 24
 8: LDN 22      ; A = b
 9: SUB 23      ; A = b - 1
10: STO 20      ; b = b - 1
11: LDN 20
12: STO 22      ; store -b
13: LDN 24      ; A = -r
14: CMP
15: JMP 25      ; r == 0 -> line 17
16: JMP 18      ; r > 0  -> line 1
17: STOP
18: NUM 0
19: NUM -989
20: NUM 988
21: NUM -3
22: NUM -988
23: NUM 1
24: NUM 0
25: NUM 16
```

(The `;` comments are illustrative; comment syntax is still open.)

## Toolchain

- GCC 13 under WSL (Ubuntu 24.04), CMake >= 3.25, C++23 (no GNU extensions).
- C++ names:
    - camelCase for functions, variables
    - PascalCase for types
    - private data members: camelCase with a trailing underscore (`store_`),
      leaving the plain name for the accessor (`store()`)
- Tests: Catch2 v3 via FetchContent, registered with CTest.
- Warnings: `-Wall -Wextra -Wpedantic -Wconversion`, applied through an
  INTERFACE target so they don't reach third-party code.
- Sanitizers: ASan + UBSan, behind the `BABY48_SANITIZE` option (default ON).
- Debug builds define `_GLIBCXX_ASSERTIONS` (bounds-checked standard containers)
- Configure, build, test:

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

## Build order

isa -> memory -> machine (hand-encoded tests) -> assembler -> end-to-end
Kilburn test -> format -> cli

## Roadmap (post-v1; each a separate, scoped release)

- v1.1: labels in the assembler (two-pass)
- v2: live terminal monitor (ncurses or FTXUI), reusing `format`
- "Baby 2": configurable store size up to 8192 words (the 13-bit address field
  was designed for this; mask = size - 1). Keep 32 as the faithful default and
  test oracle.
- "Baby 3": memory-mapped input, one word per key, latched
  at 1 until the program stores 0 to acknowledge.
- "Baby Unbounded": arbitrary store size, possibly selected at run time.
  Requires a different `memory` module and also a wider instruction format (a
  different `isa`), since the S field limits how many lines an instruction can
  address.

## References

- Burton, SSEM Programmer's Reference Manual:
  http://curation.cs.manchester.ac.uk/computer50/www.computer50.org/mark1/prog98/ssemref.html
- Tootill, "The Original Original Program", Resurrection issue 20:
  https://www.computerconservationsociety.org/resurrection/res20.htm
- HASE SSEM model (amended program, demo program):
  https://www.icsa.inf.ed.ac.uk/research/groups/hase/models/ssem/index.html
- Digital60 notes (instruction counts, reconstruction details):
  https://curation.cs.manchester.ac.uk/digital60/www.digital60.org/about/glossary/notes.html
