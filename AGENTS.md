# AGENTS.md

## Project
- C++17 Pinochle. Active build: `src/main.cpp`, `Pin.*`, `card.*`, `aiPlayer.*`.
- `Pin2.*`, `main2.cpp` = alternate impl, NOT in CMake target — ignore/don't build.
- `src/test.cpp` = placeholder, not a real suite.
- `build/` is gitignored — never commit generated files.

## Build & Run
- No CTest target configured yet. If adding tests, register in `CMakeLists.txt` and run via `ctest --test-dir build --output-on-failure`.

## C++ Style
- **Indent:** 4 spaces, no tabs.
- **Braces:** Egyptian style (open brace same line, space before).
- **Line length:** 80–100 chars.
- **Naming:**
  - New classes → `PascalCase` (legacy `card` is lowercase — don't rename).
  - `snake_case` for utility/card functions (`initialize_deck`, `count_meld`, `print_card`).
  - `camelCase` for AI/trick-flow functions (`chooseMove`, `startRound`).
  - Variables → `camelCase` (`trumpSuit`, `followSuit`).
- **Headers:** one class per header/source pair; include guards match filename uppercase (`CARD_H`).
- **Include order:** own header first → other project headers → stdlib alphabetical.
- Prefer STL containers/algorithms over raw implementations.
- Mark non-mutating member functions `const`.
- Pass non-primitives as `const&`.
- Use range-based `for` loops.
- Avoid global state.

## JavaScript Style
- 4-space indent, always semicolons, single quotes (template literals for interpolation).
- `const` by default, `let` when reassigned, **never** `var`.
- `camelCase` vars/functions, `PascalCase` classes, `UPPER_SNAKE_CASE` global constants.
- Arrow functions for callbacks/anonymous fns.
- Default params instead of `undefined` checks.
- Trailing commas in multiline objects/arrays.
- Destructure when accessing multiple props.
- Strict equality (`===`/`!==`) only.
- Simple ternaries only — no nesting.
- Method shorthand syntax in classes/objects.

## Testing Guidelines
- Add tests for: card comparison, rank/suit display, dealing, bidding, meld scoring, trick resolution.
- Name after component: `card_test.cpp`, place in `tests/` once suite exists.
- Cover boundary + invalid-value cases, not just happy path.
- Every bug fix → add a regression test (once framework configured).

## Commits & PRs
- Imperative, concise commit subjects (e.g., `Add meld scoring tests`).
- One logical change per commit.
- PRs must state gameplay/architectural impact, list build/test commands run, link related issues.
- Include terminal output/screenshots for interactive-behavior changes.
- Flag any Pinochle rules assumptions for reviewer verification.

## Three-Tier Rules

**Always**
- Compile with warnings enabled locally when practical.
- Match existing style in the file you're editing over generic conventions.
- Keep one declaration per header/source pair.

**Ask First**
- Before introducing a formatter/linter (none currently configured).
- Before adding new global constants or shared state patterns.

**Never**
- Never use `var` in JS.
- Never use loose equality (`==`/`!=`).
- Never commit `build/` artifacts.
- Never rename established public types (`Pin`, `card`) for "consistency."
- Never introduce global state in C++.