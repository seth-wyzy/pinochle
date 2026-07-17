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

## Basic C++ Style Guidelines

This document outlines basic style guidelines for writing clean, consistent, and maintainable C++ code based on the existing codebase.

### 1. Formatting & Indentation

*   **Indentation:** Use 4 spaces for indentation. Do not use tabs.
*   **Braces:** Use "Egyptian" style braces (open brace on the same line as the control statement/function signature with a preceding space, close brace on its own line).
*   **Line Length:** Keep lines under 80-100 characters for readability.

```cpp
// Good
void print_card() const {
    if (rank == 9) {
        std::cout << "9";
    }
}

// Bad
void print_card() const
{
  if (rank == 9) std::cout << "9";
}
```

### 2. Naming Conventions

*   **Types & Classes:** Match the component style. Legacy code has mixed casing (`card` is lowercase, while `Pin` and `AIPlayer` are `PascalCase`). Prefer `PascalCase` for new classes.
*   **Methods & Functions:** Match the style of the file/class:
    *   Use `snake_case` (e.g., `initialize_deck()`, `count_meld()`) for utility functions and card-related helpers in `Pin` or `card`.
    *   Use `camelCase` (e.g., `chooseMove()`, `startRound()`) for AI players and trick-taking flow controls.
*   **Variables:** Use `camelCase` or lowercase for local variables and member variables (e.g., `trumpSuit`, `followSuit`, `deck`).

### 3. File Structure & Header Rules

*   **One Class per File:** Maintain one class declaration per header/source pair.
*   **Include Guards:** Always protect headers with standard `#ifndef` / `#define` / `#endif` include guards matching the filename in uppercase (e.g., `CARD_H`).
*   **Include Ordering:**
    1. Include the corresponding project header first (e.g., `#include "Pin.h"` first in `Pin.cpp`).
    2. Include other project-specific headers (e.g., `#include "card.h"`).
    3. Include standard library headers in alphabetical order.

```cpp
// Good: Pin.cpp
#include "Pin.h"
#include "card.h"
#include <algorithm>
#include <vector>
```

### 4. Language Standard & Idioms

*   **Standard Library:** Prefer standard-library containers (`std::vector`, `std::map`) and algorithms (`std::sort`, `std::shuffle`) over raw implementations.
*   **Const Correctness:** Use `const` for member functions that do not modify class state (e.g., `void print_card() const;`).
*   **Pass-by-Reference:** Pass non-primitive parameters by reference-to-const to avoid copying (e.g., `const std::vector<card>& hand`).
*   **Modern Loops:** Use range-based `for` loops (`for (const auto& item : container)`) for clarity and safety.

## Basic JavaScript Style Guidelines

This document outlines basic style guidelines for writing clean, consistent, and maintainable JavaScript code.

### 1. Formatting & Indentation

*   **Indentation:** Use 4 spaces for indentation. Do not use tabs.
*   **Semicolons:** Always use semicolons to terminate statements.
*   **Line Length:** Keep lines under 80-100 characters for readability.
*   **Quotes:** Use single quotes (`'`) for strings, unless the string contains single quotes, in which case double quotes (`"`) or template literals (`` ` ``) are acceptable. Use template literals for string interpolation.

```javascript
// Good
const message = 'Hello, world!';
const greeting = `Hello, ${name}!`;

// Bad
const message = "Hello, world!";
```

### 2. Variables and Constants

*   **Declaration:** Always use `const` or `let`. Never use `var`.
*   **Immutability:** Default to `const`. Only use `let` when you know the variable's value will change.
*   **Naming:** Use `camelCase` for variable names.

```javascript
// Good
const MAX_COUNT = 10; // UPPER_SNAKE_CASE for global constants
let currentCount = 0;
const userName = 'Alice';

// Bad
var count = 0;
let user_name = 'Alice';
```

### 3. Functions

*   **Naming:** Use `camelCase` for function names. Names should typically be verbs or action phrases.
*   **Arrow Functions:** Use arrow functions (`() => {}`) for anonymous functions and callbacks, especially when you need to preserve the lexical scope of `this`.
*   **Default Parameters:** Use default parameters instead of checking for `undefined`.

```javascript
// Good
function calculateArea(width, height = 10) {
    return width * height;
}

const numbers = [1, 2, 3];
const doubled = numbers.map(n => n * 2);

// Bad
function calculate_area(width, height) {
  height = height || 10;
  return width * height;
}
```

### 4. Objects and Arrays

*   **Trailing Commas:** Include trailing commas in multiline object and array literals. This makes version control diffs cleaner.
*   **Destructuring:** Use object and array destructuring when accessing multiple properties or elements.

```javascript
// Good
const user = {
    firstName: 'John',
    lastName: 'Doe',
    age: 30, // Trailing comma
};

const { firstName, lastName } = user;

// Bad
const user = {
  firstName: 'John',
  lastName: 'Doe',
  age: 30
};
const firstName = user.firstName;
```

### 5. Control Flow

*   **Equality:** Always use strict equality (`===` and `!==`) instead of loose equality (`==` and `!=`).
*   **Ternary Operators:** Use ternary operators for simple conditional assignments, but avoid nesting them.

```javascript
// Good
if (count === 0) {
    // ...
}

const status = isActive ? 'Active' : 'Inactive';

// Bad
if (count == 0) {
  // ...
}
```

### 6. Classes

*   **Naming:** Use `PascalCase` for class names.
*   **Methods:** Use method shorthand syntax in object literals and classes.

```javascript
// Good
class UserProfile {
    constructor(name) {
        this.name = name;
    }

    getName() {
        return this.name;
    }
}
```
