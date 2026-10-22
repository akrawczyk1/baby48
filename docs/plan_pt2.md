# Milestone 2: `machine` module

Implements the Baby's execution semantics: the store, the accumulator, CI, and
`step()`. Starts by moving the store's shape into its own `memory` module.
Introduces a compiled (non-header-only) library, a class with private state,
and tests that run small hand-encoded programs. Ends with Kilburn's
highest-factor program running on hand-encoded memory images. Each step ends
with a verification and leaves the build working.

- Prerequisite: milestone 1 complete (toolchain, `isa`, Catch2 tests passing).
- Following milestones: assembler -> Kilburn end-to-end -> format -> cli.

---

## 1. Machine decisions
The behaviour that the tests depend on, settled before writing code and
recorded in `docs/design.md`:

- **`step()` on a stopped machine.** A halted machine returns `Halted` from
  `step()` and changes nothing: no CI increment, no instruction counted. A
  faulted machine likewise keeps returning `Fault`. Rationale: run loops can call
  `step()` until it stops returning `Ok`, with no special cases.
- **When a CI fault is detected.** Immediately after the CI increment, before
  the fetch. A step that faults executes nothing and is not counted. Rationale:
  - It catches both ways CI can gain bits 13-15: a jump, or incrementing past
    `0x1FFF`.
  - It handles the indirect-jump idiom correctly. A jump to line 0 stores
    `target - 1 = -1` (`0xFFFFFFFF`), which wraps to 0 on the increment. A check
    after execution would wrongly report that as a fault.
- **Instruction count** includes the STOP instruction, matching the 21,387
  reference count.
- **The store's shape lives in its own `memory` module,** not in `isa`. Both
  `machine` and `assembler` use `MemoryImage`, and they must not depend on each
  other. `isa` describes instruction encoding only. Keeping memory separate
  allows a future store whose size differs from the instruction format's
  address range.
- **Private data members** use camelCase with a trailing underscore (`store_`),
  which frees the plain name for the accessor (`store()`).

**Verify:** `design.md` has no OPEN item for `step()` on a halted machine, and
its Architecture, Fidelity decisions and Toolchain sections reflect the above.

## 2. The `memory` module: line count versus store size
**Objective:** separate two values that coincide in the faithful Baby, and a
second header-only library.

- Two different facts are currently both expressed as `isa::storeSize`:
  - **how many lines an instruction can address**, a property of the encoding
    (the 5-bit S field gives 32);
  - **how many words the store holds**, a property of memory.

  They are equal in the faithful Baby but need not be in a variant, so each
  gets its own constant in its own module.
- `isa.hpp`: replace `storeSize` with `inline constexpr std::size_t lineCount =
  lineMask + 1;`. `encode`'s precondition becomes `assert(line < lineCount)`.
  Tests that loop over line numbers use `isa::lineCount`.
- New `src/memory/memory.hpp`, in `namespace baby48::memory`:
  - `inline constexpr std::size_t storeSize = 32;`
  - `using MemoryImage = std::array<std::uint32_t, storeSize>;`
  - It includes only standard headers. It does not depend on `isa`.
- `using Name = Type;` is the modern spelling of `typedef`. It creates another
  name for the same type, not a new type.
- `src/CMakeLists.txt`: `add_library(baby48_memory INTERFACE)` and
  `target_include_directories(baby48_memory INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})`,
  the same pattern as `baby48_isa`. Link the tests to it.

**Verify:**
- Tests: `isa::lineCount == 32`,
  `memory::MemoryImage{}.size() == memory::storeSize`, and a
  default-constructed image is all zeros.
- `memory.hpp` compiles when it is the only file included.
- `isa.hpp` no longer mentions `storeSize`.
- All existing tests still pass.

## 3. A compiled library, and splitting the tests
**Objective:** the header/source split, `STATIC` libraries, and `PUBLIC` versus
`PRIVATE` dependencies.

- New files `src/machine/machine.hpp` (declarations) and
  `src/machine/machine.cpp` (definitions). `machine.cpp` includes its own header
  first, so the header is proven to compile on its own.
- `src/CMakeLists.txt`:
  - `add_library(baby48_machine STATIC machine/machine.cpp)`. A `STATIC`
    library compiles its sources once into an archive (`libbaby48_machine.a`)
    that executables link.
  - `target_include_directories(baby48_machine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})`
  - `target_link_libraries(baby48_machine PUBLIC baby48_isa baby48_memory PRIVATE baby48_options)`
- `PUBLIC` versus `PRIVATE`:
  - `PUBLIC`: used by this target **and** passed on to anything that links it.
    `machine.hpp` includes `isa/isa.hpp` and `memory/memory.hpp`, so every
    consumer of `machine` needs both include paths. That makes `baby48_isa` and
    `baby48_memory` `PUBLIC`.
  - `PRIVATE`: used only to build this target. The warning flags apply to
    `machine.cpp`, but they are not forced on consumers.
  - Rule of thumb: a dependency that appears in the target's headers is
    `PUBLIC`. One used only in its `.cpp` files is `PRIVATE`.
- Pitfall: because `baby48_options` is `PRIVATE` here, its sanitizer **link**
  flags do not reach executables through `baby48_machine`. Every executable
  must link `baby48_options` itself, as `hello` and the test executables
  already do.
  Otherwise linking fails with undefined `__asan_*` symbols.
- Split the tests into one executable per module. `git mv tests/tests.cpp
  tests/isa_tests.cpp` keeps the file's history. Add `tests/memory_tests.cpp`
  and `tests/machine_tests.cpp`, with executables `baby48_isa_tests`,
  `baby48_memory_tests` and `baby48_machine_tests`. Each links Catch2,
  `baby48_options`, and only the module it tests.
- `catch_discover_tests` takes a single target. Call it once per test
  executable. A second target passed to the same call is silently ignored, and
  its tests never reach CTest.
- With one executable per module, tags are not needed to select a module. Keep
  tags for groups that cut across tests, such as `[kilburn]` and `[slow]`.
- Every module shares the `src/` include root, so a test executable can still
  see headers of modules it does not link. Link lists document dependencies but
  do not enforce them.
- Start `machine.hpp` with just the namespace `baby48::machine`. A library with
  an almost empty source file builds fine.

**Verify:**
- `cmake --build build -v` shows `ar` creating `libbaby48_machine.a`, and the
  link line for `baby48_machine_tests` includes it, along with the sanitizer
  flags.
- All existing tests still pass.
- `ctest --test-dir build` lists the tests from all three executables.

## 4. `Machine`: state, construction, accessors
**Objective:** a class that protects its invariants, `explicit` constructors,
default member initializers, and `const` accessors.

- `enum class StepResult { Ok, Halted, Fault };`
- `class Machine`:
  - `explicit Machine(const memory::MemoryImage& image);` copies the image into
    the store. Registers start at zero.
  - `StepResult step();`
  - `const` accessors: `accumulator()`, `controlInstruction()` (both `std::uint32_t`),
    `store()` (returns `const memory::MemoryImage&`), `status()` (the current
    `StepResult`), `instructionCount()` (`std::uint64_t`).
  - Private members: `store_`, `accumulator_`, `controlInstruction_`, `status_`,
    `instructionCount_`.
- `static_assert(memory::storeSize == isa::lineCount);` in `machine.hpp`. This
  is where the two modules meet. The machine indexes the store with line numbers
  (CI's low bits and each operand S), which is only safe when every addressable
  line exists. A future variant that breaks this fails to compile here, rather
  than reading past the end of the store at run time.
- Concepts:
  - `class` rather than `struct`: the state is private, so it can only change
    through `step()`. Nothing outside the class can create an impossible state.
  - Default member initializers (`std::uint32_t controlInstruction_ = 0;`) keep the reset
    values next to the declarations. The constructor's member initializer list
    (`: store_(image)`) handles the one member that needs an argument.
  - `explicit` prevents an accidental implicit conversion from `MemoryImage` to
    `Machine` in a function call.
  - `store()` returns a `const` reference: the caller reads the machine's own
    array without copying it, and cannot modify it. The reference is valid only
    while the `Machine` exists.
  - Member functions defined inside the class body are implicitly `inline`, so
    one-line accessors can live in the header. `step()` belongs in
    `machine.cpp`.
- Until step 5, `step()` can simply return `status_`.
- Test helpers in `machine_tests.cpp`, inside an unnamed namespace (internal
  to the file; `namespace detail` is the header convention):
  - `imageBuilder`, taking an initializer list of `{line, word}` pairs, so a test
    program reads as a short list such as
    `{{1, encode(LDN, 20)}, {2, encode(STOP, 0)}, {20, 5}}`. It stores with
    `.at()`, so a mistyped line number fails the test instead of causing
    undefined behaviour;
  - `toTwosComplement`, which writes signed data:
    `static_cast<std::uint32_t>(std::int32_t{-5})` is well-defined and gives
    the two's complement bit pattern;
  - `runHelper`, which calls `step()` until it returns something other than
    `Ok`, with a step limit so a bug cannot hang the test run. It returns the
    final `StepResult`, so a test can `REQUIRE` a halt before checking state.

**Verify:** a new `Machine` has A = 0, CI = 0, instruction count 0, status
`Ok`, and a store equal to the image.

## 5. `step()`: the cycle and the straight-line instructions
**Objective:** the fetch-decode-execute cycle, and wraparound arithmetic with
no undefined behaviour.

- Order inside `step()`:
  1. If `status_` is not `Ok`, return it.
  2. Increment `controlInstruction_`.
  3. If CI bits 13-15 are nonzero (reuse `isa::functionShift` and
     `isa::functionMask`), set `status_` to `Fault` and return.
  4. Fetch `store_[controlInstruction_ & isa::lineMask]` and `isa::decode` it.
  5. Increment the instruction count.
  6. Execute: a `switch` on the opcode with all seven cases and no `default`,
     so `-Wswitch` flags any missing opcode. STOP sets `status_` to `Halted`.
  7. Return `status_`.
- Write the whole `switch` now. This step tests LDN, STO, SUB and STOP, and step
  6 tests the control-flow instructions.
- Arithmetic stays in `std::uint32_t` throughout. Unsigned arithmetic wraps
  modulo 2^32 by definition, which is exactly the Baby's "carries lost". Unary
  minus on a `uint32_t` (or `0u - x`) is well-defined. The same operations on
  `std::int32_t` would be undefined behaviour for `INT32_MIN`.

**Tests:**
- LDN of 5 gives A = -5 as a bit pattern. LDN of 0 gives 0.
- LDN of `0x80000000` gives `0x80000000`, with no UBSan report.
- SUB: 0 - 1 gives `0xFFFFFFFF`.
- STO writes A to its line and leaves every other line unchanged.
- STOP returns `Halted`. CI then holds STOP's line, and the count includes the
  STOP.
- Function 5 executes as SUB. Encode the word by hand: `(5u << 13) | s`.
- The first instruction executed is line 1, not line 0.

**Verify:** all of the above pass under the sanitizers.

## 6. Control flow: JMP, JRP, CMP
**Objective:** indirect and relative jumps, and the CI pre-increment.

- JMP: `controlInstruction_ = store_[s]`. The stored value is the target minus 1, because the
  next step increments CI before fetching.
- JRP: `controlInstruction_ += store_[s]`, wrapping. A negative stored value jumps backwards.
- CMP: if A is negative, increment `controlInstruction_` to skip the next instruction.
  "Negative" means bit 31 is set: `static_cast<std::int32_t>(a) < 0`
  (well-defined since C++20) or `(a >> 31) != 0`.
- Testing technique: put STOP on every possible path. The CI value after the
  halt shows which path ran.

**Tests:**
- JMP with `store[20] = 5`: execution continues at line 6, not line 2. Halt CI is
  6.
- JMP to line 0 through a stored -1: CI wraps to 0 with no fault, and line 0
  executes.
- JRP forward: line 1 is `JRP 20` with `store[20] = 2`. Line 4 runs next.
- JRP backward: reach line 5 (for example via JMP), where `JRP 21` with
  `store[21] = -3` sends execution to line 3. This is the inner loop of
  Kilburn's program.
- CMP with lines 1 `LDN 20`, 2 `CMP`, 3 `STOP`, 4 `STOP`:
  - `store[20] = 5` gives A = -5, which skips, so the halt CI is 4.
  - `store[20] = 0` gives A = 0, which does not skip, so the halt CI is 3.
  - `store[20] = -5` gives A = 5, which does not skip, so the halt CI is 3.
  - `store[20] = 0x80000000` gives A = `0x80000000`, the most negative value,
    which skips.

**Verify:** all pass. Breaking CMP on purpose, for example to `A <= 0`, makes
the A = 0 test fail.

## 7. Status edge cases
**Tests:**
- `step()` after a halt returns `Halted` and leaves A, CI, the store and the
  count unchanged.
- Fault through JMP: `store[20] = 0x2000` (bit 13 set), line 1 `JMP 20`. The
  first step returns `Ok`. The second returns `Fault` without executing, so the
  count stays 1. Further steps keep returning `Fault`.
- Fault through JRP: line 1 `JRP 20` with `store[20] = -5` takes CI below zero
  (`0xFFFFFFFC`). The next increment leaves bits 13-15 set, giving `Fault`.
- CI wraps from line 31 to line 0 without a fault. Jump to line 31 (through a
  stored 30), execute a non-jump instruction there, and place STOP on line 0.
  The halt CI is 32, not 0: CI is a full 32-bit register, and only the fetch
  masks it to 5 bits.

**Verify:** all pass. Moving the fault check to after execution makes the
"JMP to line 0 through -1" test from step 6 fail, which confirms why the check
sits where it does.

## 8. Kilburn's program on hand-encoded images
**Objective:** a complete historical program running on the machine, before the
assembler exists.

- A test helper builds the image for a given `a` from the listing in
  `design.md`:
  - lines 1-17 come from `isa::encode` (CMP and STOP with line 0);
  - data: line 18 = 0, 19 = -a, 20 = a - 1, 21 = -3, 22 = -(a - 1), 23 = 1,
    24 = 0, 25 = 16.
- Run to completion with a step limit well above the largest expected count
  (for example 3,000,000), and `REQUIRE` the result to be `Halted`.
- Expected results. CI is 17 at the halt in every case.

  | a | line 20 | instructions (including STOP) |
  |---|---|---|
  | 989 | 42 | 21,387 |
  | 3141 | 1046 | 37,174 |
  | 4537 | 348 | 88,333 |
  | 262,144 (2^18) | 131,071 | 2,228,228 |

- `GENERATE(table<...>({...}))` from `<catch2/generators/catch_generators.hpp>`
  runs one test case per row and reports the row on failure.
- Alternatively, write one test case per value. Each then appears separately
  in CTest and in the editor's test list.
- Tag these tests `[kilburn]`. The 2^18 case runs 2.2 million instructions, in
  about 0.2 s under the sanitizers. A `[slow]` tag lets
  `baby48_machine_tests "~[slow]"` skip it if it ever becomes slow.
- The counts for 3141 and 4537 and the 2^18 count come from simulating the spec
  in this document, not from historical sources. The 989 count is independently
  cross-checked.

**Verify:** all four cases pass. Changing a single data word, for example
line 21 to -2, makes them fail.

## 9. Record the results
- `design.md`, Tests section:
  - add the instruction counts for a = 3141 and 4537;
  - update the 2^18 note: the count agrees with simulation, though not with an
    external emulator.
- Optional: replace the scratch code in `src/main.cpp` with a small demo. Build
  Kilburn's image for a = 989, run it, and print line 20 and the instruction
  count. It is the first time the emulator runs a real program end to end. The
  `cli` milestone later replaces it.

---

## Milestone check
From the repository root, in WSL:
```
rm -rf build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```
Expected: a clean build with no warnings, every test in the three test
executables passing (including the four `[kilburn]` cases), and no sanitizer
reports.
