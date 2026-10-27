# Aggressive keyboard performance implementation plan

> **For agentic workers:** Use superpowers:executing-plans for the keyboard work and superpowers:dispatching-parallel-agents for the independent Nexus and HSV work.

**Goal:** Reduce the cost of keyboard scans, calibration, HSV conversion, and Nexus processing without changing their behavior.

**Architecture:** Keep the public APIs, weak board overrides, and configuration options. Reduce repeated work and function boundaries in existing hot paths; retain only changes supported by behavior checks and measurements.

**Tech Stack:** Portable C, C++ GoogleTest, CMake, AppleClang host benchmarks, GCC ARM Cortex-M code generation.

**Spec:** The user's request: branch from `codex-fable` as `codex-aggressive-performance`; prioritize the six keyboard/calibration/HSV/Nexus workloads; preserve behavior; commit and push without a co-author.

## Global constraints

- AT32F405 is the main MCU; retain support for other MCUs.
- Preserve exact integer rounding, calibration direction, debounce, callbacks, event order, and wire/report bytes.
- Keep board overrides of raw sampling and normalization callable.
- Preserve benchmark workloads between baseline and candidate; label host timings as host timings.

## Review focus

- Runtime changes to modes, bounds, layers, and keymaps must take effect at the same point in a tick.
- Rapid-trigger extrema and signed speed differences must remain identical at threshold boundaries.
- Nexus mappings may contain duplicates, invalid slots, and keys changed outside Nexus.
- Invalid HSV hues and clamped saturation/value must retain the existing output.
- Calibration must match all positive, negative, and zero spans, including maximum LUT sizes.

### Task 1: Keyboard scan and calibration

Files: `src/advanced_key.c`, `src/keyboard.c`, `src/keyboard.h`, `test/advanced_key/`, `test/keyboard/`, `test/CMakeLists.txt`.

- [x] Preserve the `codex-fable` benchmark executable in `build/codex-aggressive-baseline/` and run the baseline test suite (159 passed).
- [x] Add deterministic reference comparisons for analog modes, raw calibration, debounce boundaries, and runtime mode changes. Run them against the unchanged implementation first.
- [x] Consolidate the analog update pipeline so the compiler can inline shared state processing; measure before keeping each candidate.
- [x] Inspect native-word calibration code and retain further changes only if exact arithmetic and useful code generation are demonstrated.
- [x] Run `cmake --build build/codex-release --parallel 4` and `ctest --test-dir build/codex-release --output-on-failure`.

### Task 2: Nexus processing

Files: `src/nexus.c`, `test/nexus/` (independent agent).

- [x] Pin externally changed key states, debounce, continuous bindings, duplicate mappings, and timeout behavior with regression checks.
- [x] Reduce per-slot work without caching assumptions that hide runtime changes; preserve bitmap, raw, and slave configurations.
- [x] Run all Nexus test variants and inspect Cortex-M4 output.

### Task 3: HSV conversion

Files: `src/color.c`, HSV checks in `test/rgb/test_rgb.cpp` (independent agent).

- [x] Use exact algebraic reductions or cheaper channel selection, preserving every output byte.
- [x] Run the exhaustive valid HSV domain and invalid/clamped-input checks.
- [x] Inspect Cortex-M4 output and keep the candidate only with useful timing/code-generation evidence.

### Task 4: Verification and delivery

Files: `test/bench/compare.py`, performance results documentation, affected test configuration as needed.

- [x] Run Release tests, focused sanitizers, configuration variants, and Cortex-M0/M3/M4 compilation.
- [x] Run alternating baseline/candidate benchmarks with other work stopped: `python3 test/bench/compare.py build/codex-aggressive-baseline/libamp_bench build/codex-release/test/libamp_bench --repeats 9 --ticks 2000000 --frames 200000 --json build/codex-aggressive-performance.json`.
- [x] Obtain an independent whole-change review and address behavior or validation issues.
- [x] Record the complete change list and measured gains/regressions; prepare the verified files for the requested commit and push without a co-author.

Results and validation are recorded in [the performance report](../../performance/aggressive.md).
