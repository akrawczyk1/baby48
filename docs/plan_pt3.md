# Milestone 3: `assembler` module, and Kilburn end to end

Turns source text such as `1: LDN 18` into a `MemoryImage`, reporting the first
error with its source line and kind. Introduces text processing with
`std::string_view`, number parsing with `std::from_chars`, error values with
`std::expected`, and a test helper header shared between test executables.
Ends with Kilburn's program assembled from its listing and run on the machine.
Each step ends with a verification and leaves the build working.

- Prerequisite: milestone 2 complete (`machine`, Kilburn on hand-encoded images).
- Following milestones: format -> cli.

---

## 1. Assembler decisions
The syntax and error model, settled before writing code and recorded in the
Assembler syntax section of `docs/design.md`:

- **Comments** start with `;` and run to the end of the line, on their own line
  or after a statement. Blank lines are ignored. Cutting each line at its first
  `;` handles both cases with the same code.
- **Numbers.** Line numbers and operands are decimal, 0-31. `NUM` values are
  signed decimal (-2147483648 to 2147483647) or `0x`-prefixed hexadecimal
  (`0x0` to `0xFFFFFFFF`, no sign). A leading `+` is rejected everywhere.
- **`CMP` and `STOP`** accept an optional operand, which must still be a valid
  operand if present. It is not encoded: the word is `encode(op, 0)`.
- **Mnemonics**, including `NUM`, are case-insensitive. `STP` is an alias for
  `STOP`.
- **`AsmError`** is a source line number plus an error kind. A line number alone
  would let a test for one kind of error pass when the assembler failed for a
  different reason; the kind makes every error test specific. User-facing
  messages belong to `cli`.
- **Source lines** are numbered from 1 and count every line of the text,
  including blank and comment lines, so they match what an editor shows.
- **Assembly stops at the first error.** `std::expected` holds one error.

**Verify:** the Assembler syntax section of `design.md` covers each point above,
and its OPEN item on comment syntax is gone.

## 2. Module skeleton and shared test support
**Objective:** a third compiled library, applying the `PUBLIC`/`PRIVATE` rule;
a test helper header shared by several test executables.

- New files `src/assembler/assembler.hpp` and `src/assembler/assembler.cpp`, in
  `namespace baby48::assembler`:
  - `enum class AsmErrorKind` with one enumerator per error kind in
    `design.md`: `MalformedLine`, `LineOutOfRange`, `DuplicateLine`,
    `UnknownMnemonic`, `MissingOperand`, `ExtraOperand`, `InvalidOperand`,
    `OperandOutOfRange`, `InvalidValue`, `ValueOutOfRange`.
  - `struct AsmError { std::size_t sourceLine; AsmErrorKind kind; }` with a
    defaulted `operator==`.
  - `std::expected<memory::MemoryImage, AsmError> assemble(std::string_view source);`
  - To start with, `assemble` returns an all-zero image.
- `src/CMakeLists.txt`: `add_library(baby48_assembler STATIC assembler/assembler.cpp)`.
  Apply the rule of thumb from milestone 2:
  - `baby48_memory` is `PUBLIC`: `assembler.hpp` uses `memory::MemoryImage`.
  - `baby48_isa` is `PRIVATE`: only `assembler.cpp` uses `isa`
    (`opcodeFromMnemonic`, `usesOperand`, `encode`).
  - `baby48_options` is `PRIVATE`.
  - The include directory is `PUBLIC`, as for `baby48_machine`.
- Because `isa` is `PRIVATE`, anything else that uses `isa` must link
  `baby48_isa` itself. The assembler tests call `isa::encode` to build expected
  images, so `baby48_assembler_tests` links `baby48_isa` directly.
- Shared test support. `imageBuilder`, `toTwosComplement` and `kilburnBuilder`
  are needed by the machine tests, the assembler tests and the end-to-end tests.
  Move them from `machine_tests.cpp` into `tests/test_support.hpp`:
  - Functions defined in a header that several `.cpp` files include must be
    `inline`, or the linker reports multiple definitions. (`constexpr`
    functions are implicitly `inline`; these are not `constexpr`.)
  - Use a named namespace such as `test_support`, not an unnamed one. An unnamed
    namespace in a header gives every including file its own private copy.
  - `runHelper` uses `machine` and stays in `machine_tests.cpp`.
  - `#include "test_support.hpp"` finds the file because it sits next to the
    test sources.
- New `tests/assembler_tests.cpp` and executable `baby48_assembler_tests`
  (Catch2, `baby48_options`, `baby48_assembler`, `baby48_isa`), with its own
  `catch_discover_tests` call.
- A `Catch::StringMaker<AsmError>` in `assembler_tests.cpp` that prints
  `line 3: DuplicateLine`, so failures name the kind rather than a number.
- Test shapes:
  - success: `REQUIRE(result.has_value())`, then compare `*result` with an
    expected image from `imageBuilder`;
  - failure: `REQUIRE_FALSE(result.has_value())`, then
    `CHECK(result.error() == AsmError{3, AsmErrorKind::DuplicateLine})`.

**Verify:**
- Assembling `""` succeeds with an all-zero image.
- The machine tests still pass after the helpers move.
- `ctest` lists the tests from four executables.

## 3. Lines, comments and whitespace
**Objective:** walking text with `std::string_view` without copying it.

- Split `source` into lines at `'\n'` with `find`. Number them from 1. The last
  line has no trailing `'\n'` and must still be processed.
- Cut each line at its first `';'` (`find`, then `substr`).
- Trim spaces, tabs and `'\r'` from both ends (`find_first_not_of`,
  `find_last_not_of`). Trimming `'\r'` makes Windows line endings work without
  special handling.
- Skip lines that are empty after trimming.
- Put these helpers (`trim`, `stripComment`) in an unnamed namespace in
  `assembler.cpp`.
- `string_view` pitfalls:
  - A view does not own its characters. Views into `source` are safe, because
    the caller's text outlives `assemble`.
  - A view into a local `std::string` dangles once the function returns.
  - `substr` on a `string_view` returns another view, not a copy.

**Tests** (each assembles successfully to an all-zero image):
- `""`, `"\n\n\n"`, `"   \t  "`;
- `"; a comment"`, `"   ; an indented comment"`;
- `"\r\n\r\n"` (Windows line endings).

**Verify:** all pass.

## 4. The line-number field
**Objective:** parsing a number from text with `std::from_chars`.

- Split each statement at its first `':'`. No `':'` gives `MalformedLine`.
- The left part, trimmed, must be a whole decimal number:
  - `std::from_chars` into an unsigned type (`<charconv>`). It throws no
    exceptions, ignores the locale, and reports problems through `errc`.
  - Success requires **both** `ec == std::errc{}` **and** the returned pointer
    being at the end of the token. `from_chars` stops at the first character it
    cannot use and reports success, so `"12abc"` parses as 12 unless the pointer
    is checked.
  - `from_chars` into an unsigned type rejects a leading `-` or `+` itself,
    with `invalid_argument`.
  - Not a number gives `MalformedLine`. A value above 31, or
    `errc::result_out_of_range`, gives `LineOutOfRange`.
- The right part, trimmed, must not be empty (`MalformedLine`). Until step 5 it
  is not interpreted further.
- Track which store lines have been used with
  `std::array<bool, memory::storeSize>`. A second use gives `DuplicateLine` on
  the later source line.

**Tests:**
- `"LDN 18"` (no colon) gives `{1, MalformedLine}`.
- `"x: STOP"`, `"-1: STOP"` and `"1:"` give `MalformedLine`.
- `"32: STOP"` and `"99999999999: STOP"` give `LineOutOfRange`.
- `"0: STOP"`, `"31: STOP"` and `"01: STOP"` are accepted.
- `"1: STOP\n1: STOP"` gives `{2, DuplicateLine}`.

**Verify:** all pass.

## 5. Mnemonics and operands
**Objective:** reusing the `isa` module instead of duplicating its knowledge.

- Split the right part into whitespace-separated tokens. Expect a mnemonic and at
  most one operand. A third token gives `ExtraOperand`.
- Look up the mnemonic with `isa::opcodeFromMnemonic`, which already handles case
  and the `STP` alias. `NUM` (any case) is handled in step 6. Anything else gives
  `UnknownMnemonic`.
- Compare `NUM` case-insensitively with a helper in the assembler's unnamed
  namespace. `isa::detail::toUpperChar` is internal to `isa` and is not used
  from another module.
- If `isa::usesOperand(op)`:
  - no operand gives `MissingOperand`;
  - an operand that is not a whole decimal number gives `InvalidOperand`;
  - a value above 31 gives `OperandOutOfRange`.
- For `CMP` and `STOP`, an operand is optional and validated in the same way
  if present. The stored word is `isa::encode(op, 0)`.
- Store `isa::encode(op, operand)` on the statement's line.

**Tests:**
- Each of the seven opcodes assembles to the word `isa::encode` produces.
  Compare whole images.
- Lowercase and mixed-case mnemonics, and `STP`, are accepted.
- `"1: CMP"` and `"1: CMP 0"` give the same image.
- `"1: LDN 18 ; clear A"` is accepted (comment after a statement).
- `"1: LDN"` gives `MissingOperand`.
- `"1: LDN 1 2"` gives `ExtraOperand`.
- `"1: LDN x"` and `"1: LDN -1"` give `InvalidOperand`.
- `"1: LDN 32"` gives `OperandOutOfRange`; `"1: LDN 31"` is accepted.
- `"1: ADD 5"` gives `UnknownMnemonic`.

**Verify:** all pass.

## 6. `NUM` values
**Objective:** two number formats, signed and unsigned parsing, and exact
boundaries.

- A `0x` or `0X` prefix means hexadecimal: parse the rest with base 16 into
  `std::uint32_t`. No sign is allowed after the prefix, and `from_chars` into
  an unsigned type rejects one.
- Otherwise the value is decimal: parse into `std::int32_t`, which accepts a
  leading `-`, rejects `+`, and reports values outside the 32-bit range as
  `errc::result_out_of_range`. Convert the result to `std::uint32_t` to store
  it (two's complement, well-defined since C++20).
- Not a number gives `InvalidValue`. Out of range gives `ValueOutOfRange`.
- A missing value gives `MissingOperand`, and a second value gives
  `ExtraOperand`, the same kinds as for instructions.
- Behaviour of `std::from_chars`, confirmed on the project's toolchain:

  | Input | Type, base | Result |
  |---|---|---|
  | `-5` | `int32_t`, 10 | ok, -5 |
  | `+5` | `int32_t`, 10 | `invalid_argument` |
  | `-2147483648` | `int32_t`, 10 | ok |
  | `2147483648` | `int32_t`, 10 | `result_out_of_range` |
  | `-5` | `uint32_t`, 10 | `invalid_argument` |
  | `12abc` | `uint32_t`, 10 | ok, 12, **only 2 of 5 characters used** |
  | `0x1F` | `uint32_t`, 10 | ok, 0, only 1 of 4 characters used |
  | `ffffffff` | `uint32_t`, 16 | ok, 4294967295 |
  | `100000000` | `uint32_t`, 16 | `result_out_of_range` |
  | (empty) | any | `invalid_argument` |

**Tests:**
- `2147483647` is accepted; `2147483648` gives `ValueOutOfRange`.
- `-2147483648` is accepted and stores `0x80000000`; `-2147483649` gives
  `ValueOutOfRange`.
- `0xFFFFFFFF` and `0X1f` are accepted; `0x100000000` gives `ValueOutOfRange`.
- `+5`, `0x`, `-0x5`, `12abc` and `five` give `InvalidValue`.
- `"1: NUM"` gives `MissingOperand`; `"1: NUM 1 2"` gives `ExtraOperand`.
- `"1: num -3"` is accepted.

**Verify:** all pass.

## 7. Error positions
**Tests:**
- An error after blank and comment lines reports the line an editor would show.
  For example, an error on the fourth line of `"; c\n\n1: STOP\n2: BAD"` gives
  `{4, UnknownMnemonic}`.
- With two bad lines, only the first is reported.
- An error on a final line with no trailing newline is reported with the right
  line number.
- Line numbers stay correct with Windows (`\r\n`) line endings.

**Verify:** all pass. Changing the line counter to start from 0 makes these
tests fail.

## 8. Kilburn end to end
**Objective:** the assembler and the machine working together, through a test
that depends on both while neither module depends on the other.

- New `tests/e2e_tests.cpp` and executable `baby48_e2e_tests`, linking Catch2,
  `baby48_options`, `baby48_assembler`, `baby48_machine` and `baby48_isa`.
  Tag the tests `[kilburn]`.
- The program text is the listing from `design.md`, comments included, in a
  raw string literal: `R"(...)"` keeps line breaks and needs no escaping.
- Checks for a = 989:
  1. `assemble(listing)` succeeds, and the image equals `kilburnBuilder(989)`
     exactly. This proves the assembler and the hand encoding agree word for
     word.
  2. Running it gives line 20 = 42, 21,387 instructions, and CI 17.
- The other values. Build the source with `std::format` (`<format>`, C++20),
  with `{}` placeholders for the three data lines that depend on a:
  - line 19 = -a;
  - line 20 = a - 1;
  - line 22 = -(a - 1).

  The format string must be known at compile time, which a raw string literal
  is. Expected results are the table in `design.md` (Tests section).

**Verify:** all four values pass end to end. Changing one operand in the
listing, for example `5: JRP 20`, makes the image comparison fail.

## 9. Record the results
- `design.md`, Tests section: the end-to-end Kilburn tests pass for all four
  values.
- Optional: replace the scratch code in `src/main.cpp` with a demo that
  assembles the Kilburn listing, runs it, and prints line 20 and the instruction
  count. The `cli` milestone later replaces it.

---

## Milestone check
From the repository root, in WSL:
```
rm -rf build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```
Expected: a clean build with no warnings, every test in the five test
executables passing (including the end-to-end `[kilburn]` cases), and no
sanitizer reports.
