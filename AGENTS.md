# Repository Guidelines

## Project Structure & Module Organization

This repository contains a C++17 implementation of Pinochle. Production code lives in `src/`. `main.cpp` is the entry point for the active `pin` executable; `Pin.*` contains game logic, `card.*` models cards, and `aiPlayer.*` contains computer-player behavior. `Pin2.*` and `main2.cpp` appear to be an alternate implementation and are not included in the current CMake target. `src/test.cpp` is presently a placeholder rather than a configured test suite. Keep generated files in `build/`, which is ignored by Git.

## Build, Test, and Development Commands

- `cmake -S . -B build` configures an out-of-source CMake build.
- `cmake --build build` compiles the `pin` executable.
- `./build/pin` runs the game locally.
- `cmake --build build --clean-first` performs a clean rebuild when stale objects are suspected.

There is currently no CTest target. If tests are added, register them in `CMakeLists.txt` so contributors can run them consistently with `ctest --test-dir build --output-on-failure`.

## Coding Style & Naming Conventions

Use C++17 and follow the style already present: four-space indentation, braces on the same line as functions and control statements, and one declaration per header/source pair. Preserve existing public type names such as `Pin` and `card`; use descriptive `snake_case` names where that convention already exists (for example, `print_card`). Protect headers with include guards and include the corresponding project header first in each `.cpp` file. Prefer standard-library facilities and avoid introducing global state. No formatter or linter is configured, so keep formatting consistent with nearby code and compile with warnings enabled during local checks when practical.

## Testing Guidelines

Add focused tests for card comparison, rank/suit display, dealing, bidding, meld scoring, and trick resolution. Name test files after the component, such as `card_test.cpp`, and place them in `tests/` when establishing the test suite. Cover boundary and invalid-value behavior as well as normal gameplay. Every bug fix should include a regression test once a framework is configured.

## Commit & Pull Request Guidelines

Recent history uses short, informal summaries. Improve on that pattern with concise, imperative subjects such as `Add meld scoring tests`; keep each commit limited to one logical change. Pull requests should explain gameplay or architectural effects, list build/test commands run, and link related issues. Include terminal output or screenshots when a change affects interactive behavior, and call out any rules assumptions reviewers should verify.
