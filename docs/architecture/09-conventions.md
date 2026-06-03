# Code conventions

`.clang-format` and `.clang-tidy` are authoritative. This document explains the *why* and covers the things tools cannot enforce.

## Naming

| Thing | Style | Example |
|---|---|---|
| Files (header + source) | `snake_case` | `archetype_storage.cpp` |
| Namespaces | `lower_case` | `engine::render` |
| Types (classes, structs, enums) | `CamelCase` | `Renderer`, `Phase2dSprite` |
| Functions, free + member | `snake_case` | `create_pipeline()` |
| Variables, parameters | `snake_case` | `int frame_count` |
| Private member data | `snake_case_` (trailing underscore) | `std::vector<Entry> entries_` |
| Constants, constexpr | `snake_case` | `constexpr int max_frames = 60` |
| Macros | `UPPER_CASE` | `ENGINE_ASSERT` |
| Concept | `CamelCase` | `concept Plugin = ...` |
| Template type parameters | `CamelCase`, short or descriptive | `template <typename T>`, `template <Plugin P>` |

## Files and headers

- `#pragma once`. No include guards.
- Header file extension `.hpp`. Source `.cpp`. Module interface units `.cppm` (when modules ship for our code in M2).
- Match the public/private split: anything not needed by callers lives in `engine/<module>/detail/`.

```
engine/ecs/world.hpp          ← public
engine/ecs/query.hpp          ← public
engine/ecs/detail/archetype.hpp  ← internal
engine/ecs/world.cpp          ← implementation
```

- Include order (clang-format enforces): C system → C++ standard → third-party → `"engine/..."` → relative.
- Forward-declare aggressively in headers. Pull full headers only in `.cpp`.

## Functions

- Prefer free functions when no state is captured. Member functions are for things that genuinely depend on the object's invariants.
- One return value, expressed via `Result<T>` if fallible, plain `T` if not.
- Out-parameters are forbidden; use a small struct return or `std::tuple` if you must.
- `[[nodiscard]]` on:
  - Any `Result<...>` returner.
  - Getters / pure functions where ignoring the result is always a bug.

## Const correctness

- `const` on local variables when not mutated, **after** declaration not before (`auto x = ...;` then if not mutated, `const auto x`). clang-tidy nags.
- `const` member functions for any function that doesn't observably mutate.
- `mutable` allowed only for caches behind a `const` interface, and must be commented as to why.

## References, pointers, optional

| Use | When |
|---|---|
| `T&` | Required non-null, lifetime guaranteed by caller |
| `const T&` | Same, read-only |
| `T*` / `const T*` | Optional, or non-owning + nullable. Comment that null is meaningful. |
| `std::optional<T>` | Optional **value** (not handle) |
| `std::unique_ptr<T>` | Single owner with deferred destruction |
| `std::shared_ptr<T>` | **Justify in a comment.** We rarely want refcounts. |
| `std::span<T>` | Function parameter for "array of T", read or write |
| `std::string_view` | Function parameter for "string of bytes", read-only |

## Standard library use

- Free to use: `vector`, `array`, `span`, `string`, `string_view`, `optional`, `expected`, `variant`, `unique_ptr`, `unordered_map` (with caveats), `format`, `print`, `chrono`, `bit`, `concepts`, `ranges`, `generator`, `source_location`.
- Use with care: `unordered_map` (default hash bad for ints — use `absl::flat_hash_map` when M4 lands, or a hand-rolled hash). `function` (allocates) — prefer `move_only_function` or `function_ref`.
- Avoid: `iostream` (slow, formatting is ugly — use `std::format` / `std::print`). `regex` (slow). `chrono` durations as parameters (use `Duration` typedef from `engine/core/time.hpp` for consistency).

## Comments

Default: don't write them. Code with good names explains itself.

Write a comment when the **why** is non-obvious:
- A hidden constraint or invariant.
- A workaround for a specific bug (link to the issue if there is one).
- An ordering requirement that the type system doesn't capture.
- Behavior that would surprise a careful reader.

Never:
- Repeat what the code does. (`// increment i` next to `++i`.)
- Reference the current PR / task / ticket. Goes in the commit message, not the code.
- "TODO" without owner + context. Either fix it now, file an issue, or leave it out.

## Includes hygiene

- One `#include` per type used directly. Don't rely on transitive includes.
- Use the angle form `<>` for external + standard, quoted `"..."` for our own.
- Forward declaration > include in headers.

## Template patterns

- Concept-constrain templates: `template <Plugin P>` not `template <typename P>`.
- Hide template implementation in the header. We're not afraid of header weight; we *are* afraid of link-time mysteries.
- For very heavy templates (e.g. ECS query iterators), explicit-instantiate the common cases in a `.cpp` and `extern template` in the header.

## Avoid

- `using namespace ...;` at file scope. Acceptable inside a function body.
- Multiple inheritance other than tag interfaces / mixins.
- Non-const global state. Use a resource on `World` instead.
- `static` non-trivially-destructible globals (init-order trap). Use Meyers singletons (`static T& instance() { static T t; return t; }`) if you must.
- Single-letter variable names except `i`, `j`, `k` for indices in a tight loop.
- "Hungarian" prefixes (`m_`, `g_`, `s_`). The trailing `_` for private members is enough.

## Formatting (enforced by clang-format)

Highlights worth knowing without checking the file:

- 4-space indent, no tabs.
- 100-column limit.
- Pointer/reference align left: `int* p`, `int& r`.
- Brace on the same line: `if (cond) {`.
- Always braces, even one-liners.

## Conventional commits

```
<type>(<scope>): <subject>

<body, wrapping at 72>

<footer: BREAKING CHANGE: ..., Refs #...>
```

Types: `feat`, `fix`, `refactor`, `docs`, `test`, `build`, `ci`, `chore`.

Scope is the directory under `engine/` or `plugins/` most affected: `feat(ecs):`, `fix(render):`, `docs(architecture):`.

The body should answer **why**, not what.

## What the linter cannot enforce (review checklist)

- Have you read the spec's "Out of scope" section before writing code?
- Does any new function take more parameters than it returns? Group them in a struct.
- Have you added a comment that restates the code? Delete it.
- Is there a `Result<T>::value()` outside tests? Replace with `if (auto r = ...; r)`.
- Did you `#include` a third-party header in a public engine header? **Fail the PR.**
- Did you add a TODO without an issue link? Fix or link.
