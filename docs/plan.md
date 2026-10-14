# Milestone 1: toolchain and `isa` module

Sets up the build, warning, sanitizer and test infrastructure, then implements
the `isa` module on top of it. Each step introduces one concept, ends with a
verification, and leaves the build working.

- Environment: GCC 13 under WSL (Ubuntu 24.04), CMake 3.28, Make.
- Tests: Catch2 v3 via FetchContent.
- Following milestones: machine -> assembler -> Kilburn end-to-end -> format -> cli.

---

## 1. Hello world with CMake
**Objective:** the difference between the configure and build steps.
- `src/main.cpp` prints a message. The top-level `CMakeLists.txt` contains
  `cmake_minimum_required(VERSION 3.25)`, `project(baby48 LANGUAGES CXX)` and
  `add_executable(...)`.
- Configure: `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug`. Build:
  `cmake --build build`. Run: `./build/<name>`.
- `build/CMakeCache.txt` holds the settings cached at configure time.
  `build/Makefile` is generated and never edited. The whole `build/` directory is
  disposable and can be deleted and regenerated.
- Optional: `set(CMAKE_EXPORT_COMPILE_COMMANDS ON)` writes
  `build/compile_commands.json`, so editor tooling (VS Code C++, clangd) uses the
  same flags as the real build.

**Verify:** `cmake --build build -v` prints the exact g++ command line.

## 2. Language standard
- `CMAKE_CXX_STANDARD 23`, `CMAKE_CXX_STANDARD_REQUIRED ON`,
  `CMAKE_CXX_EXTENSIONS OFF`. Without the last setting, CMake passes
  `-std=gnu++23` rather than `-std=c++23`.

**Verify:** the `-v` output shows `-std=c++23`, and code using `std::expected`
compiles.

## 3. Warnings through an INTERFACE target
- `add_library(baby48_options INTERFACE)`, then
  `target_compile_options(baby48_options INTERFACE -Wall -Wextra -Wpedantic -Wconversion)`,
  then link each project target to it.
- Rationale: an INTERFACE target applies the flags only to targets that link it.
  Global flags would also reach third-party code such as Catch2.

**Verify:** assigning a `std::uint32_t` to a `std::uint8_t` produces a
`-Wconversion` warning. Remove the test code afterwards.

## 4. Sanitizers behind an option
- `option(BABY48_SANITIZE "..." ON)`. Inside `if(BABY48_SANITIZE)`:
  - compile options `-fsanitize=address,undefined -fno-omit-frame-pointer`;
  - link options `-fsanitize=address,undefined`, because the sanitizer runtimes
    are needed at link time.
- Optional: `-D_GLIBCXX_ASSERTIONS`, either always or in Debug only via
  `$<$<CONFIG:Debug>:...>`. It enables bounds checks in
  `std::array::operator[]`, which give a clearer failure than an ASan report.

**Verify:**
1. An out-of-bounds `std::array` access through `.data()` produces an ASan
   report. A signed `int` overflow produces a UBSan runtime error. Derive the
   index and the overflowing value from a runtime input such as `argc`. With
   constants, GCC may diagnose the bug at compile time (`-Warray-bounds`,
   `-Woverflow`) or fold it away, and the runtime report never appears.
2. With `-DBABY48_SANITIZE=OFF`, the sanitizer flags no longer appear in the
   `-v` output. Reconfigure with the option back on afterwards.

Known issue: on some recent WSL kernels, ASan binaries loop printing
`AddressSanitizer:DEADLYSIGNAL` because `vm.mmap_rnd_bits` is set too high.
`sudo sysctl vm.mmap_rnd_bits=28` fixes it until the next reboot.

## 5. Repository hygiene and first commit
- `.gitignore`: `build/`.
- `.gitattributes`: `* text=auto eol=lf`, so line endings stay LF even when a
  file is edited from Windows.
- First commit: `.gitignore`, `.gitattributes`, `CMakeLists.txt`, `src/`, `docs/`.

**Verify:** `git status` is clean and does not list `build/`.

## 6. Project layout and the `isa` module
- Top-level `CMakeLists.txt`: `add_subdirectory(src)`.
- `src/CMakeLists.txt`: `add_library(baby48_isa INTERFACE)` (header-only) and
  `target_include_directories(baby48_isa INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})`,
  so that code can write `#include "isa/isa.hpp"`.
- `src/isa/isa.hpp`, in `namespace baby48::isa`, with everything `constexpr`:
  - `enum class Opcode : std::uint8_t`: JMP=0, JRP=1, LDN=2, STO=3, SUB=4,
    CMP=6, STOP=7. Function 5 has no enumerator.
  - Constants: store size 32, line mask `0x1F`, function shift 13, function
    mask `0x7`. A `std::array` listing every opcode, for iterating in tests.
  - `struct Instruction { Opcode op; std::uint8_t line; }` with a defaulted
    `operator==`.
  - `encode(Opcode, std::uint8_t) -> std::uint32_t`, precondition
    `assert(line < 32)`. An `assert` inside a constexpr function only breaks
    constant evaluation when it fails.
  - `decode(std::uint32_t) -> Instruction`. F=5 maps to SUB, and all bits
    outside S and F are ignored.
  - `mnemonic(Opcode) -> std::string_view`.
  - `opcode_from_mnemonic(std::string_view) -> std::optional<Opcode>`. Open
    decision: whether matching is case-sensitive.
  - `uses_operand(Opcode) -> bool`, false for CMP and STOP.
- Pitfalls:
  - Under `-Wconversion`, the `uint32_t -> uint8_t` narrowing in `decode` needs
    an explicit `static_cast`.
  - A `switch` that covers every enumerator still triggers `-Wreturn-type`.
    `std::unreachable()` (C++23, `<utility>`) after the `switch` silences it.

**Verify:** `static_assert(encode(Opcode::LDN, 18) == 16402);` compiles. A
header-only library compiles nothing by itself, so the `static_assert` has to
sit in a translation unit that includes `isa/isa.hpp`: either `src/main.cpp`,
with that executable linked to `baby48_isa`, or the bottom of `isa.hpp`, where it
also serves as documentation. Otherwise errors in the header go unnoticed until
step 7.

## 7. Catch2 wiring with a placeholder test
- Top level: `enable_testing()` before `add_subdirectory(tests)`. Called inside
  `tests/`, it leaves `ctest --test-dir build` with nothing to run.
- `tests/CMakeLists.txt`:
  - `include(FetchContent)`
  - `FetchContent_Declare(Catch2 GIT_REPOSITORY https://github.com/catchorg/Catch2.git GIT_TAG v3.8.1 GIT_SHALLOW TRUE SYSTEM)`.
    The tag is pinned. `SYSTEM` (CMake 3.25+) treats Catch2's headers as system
    headers, so project warnings don't fire inside them.
  - `FetchContent_MakeAvailable(Catch2)`
  - `add_executable(baby48_tests ...)` linked to `Catch2::Catch2WithMain`
    (which provides `main()`), `baby48_isa` and `baby48_options`.
  - `include(Catch)` and `catch_discover_tests(baby48_tests)`, which register each
    `TEST_CASE` with CTest.
- Placeholder test: `CHECK(1 == 1)`.
- The first build takes about a minute while Catch2 compiles. Later builds reuse
  the result.
- `catch_discover_tests` runs the test binary right after linking, to list its
  tests. If that binary crashes, for example on an ASan error at startup, the
  build fails rather than the test run.

**Verify:** `build/_deps/` contains the fetched Catch2 source.
`ctest --test-dir build --output-on-failure` reports 1 passing test. Changing the
check to `CHECK(1 == 2)` makes it fail.

## 8. `isa` tests
Test cases:
- `encode(LDN, 18) == 16402`, the reference value from the design doc.
- `decode(encode(op, s)) == Instruction{op, s}` for every opcode and all 32
  lines.
- F=5 decodes as SUB.
- ORing `0xFFFF1FE0` (every bit outside S and F) into an encoded word doesn't
  change the result of `decode`.
- `mnemonic` and `opcode_from_mnemonic` round-trip. Unknown text (`"NUM"`, `""`,
  and lowercase text if matching is case-sensitive) gives `nullopt`.
- `uses_operand` is true for the five memory operations and false for CMP and
  STOP.

Specialising `Catch::StringMaker<Instruction>` in the test file makes failures
print as `LDN 18` rather than `{?}`.

**Verify:** breaking `decode` on purpose, for example by dropping the F=5
mapping, makes at least one test fail.

## 9. Record the decisions
Update the Toolchain section of `docs/design.md` to match what was built: the
compiler and standard, the Catch2 version, the warning and sanitizer flags, the
`BABY48_SANITIZE` option, and the configure, build and test commands.

---

## Milestone check
From the repository root, in WSL:
```
rm -rf build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```
Expected: a clean build with no warnings, all `isa` tests passing, and no
sanitizer reports.
