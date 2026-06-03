# 0001 — C++23 baseline with selective C++26 adoption

- Status: superseded by ADR 0002 (Fedora-only) on 2026-06-02
- Date: 2026-06-02

> **Update 2026-06-02:** ADR 0002 narrows the supported platform to Fedora only through M4. With GCC 16 + Clang 20 as the sole compilers, the baseline is now **C++26**, not C++23. The original C++23 reasoning below is preserved for context; the multi-compiler floor and `__cpp_*` fallback requirement no longer apply in their original strict form. They will return at M5 when Windows + macOS re-enter the matrix.

## Context

The engine targets modern C++. As of mid-2026, C++26 is partially shipped across the three compilers we support (GCC 15, Clang 20, MSVC 19.4x). Static reflection (P2996), pattern matching (P2688), and contracts (P2900) are not production-ready in any compiler. `std::execution` (P2300) and named modules are usable but uneven.

We need a single language baseline that compiles cleanly across our full CI matrix on day one, without forcing every developer (and every AI agent) onto bleeding-edge nightly toolchains.

## Decision

- **Baseline:** `-std=c++23` / `/std:c++latest`.
- **C++26 adoption:** opt-in only, guarded by `__cpp_*` feature-test macros, with a working C++23 fallback for every feature used.
- **`std::execution`:** use the `stdexec` reference implementation instead of the stdlib version — uniform behavior across the matrix.
- **Reflection / pattern matching / contracts:** not used in shipping code until at least 2027.
- **Compiler floor:** GCC 15, Clang 20, MSVC 19.40 (VS 2022 17.10+). CMake 3.30+, Ninja 1.12+.

## Consequences

- Positive: every supported compiler builds the engine on day one. Agent feedback loop is not bottlenecked on toolchain bugs.
- Positive: C++26 features can be evaluated in isolation behind a macro before being adopted broadly.
- Negative: lose the ergonomic wins of reflection (must hand-write `system_traits<F>` for ECS system parameter introspection). Acceptable tradeoff for now.

## Alternatives considered

- **Pure C++26 with a single-vendor toolchain (Clang only).** Rejected: violates the cross-platform requirement and locks us out of MSVC-native debugging on Windows.
- **C++20 baseline.** Rejected: gives up `std::expected`, `std::print`, `std::generator`, deducing-this, `if consteval`, ranges-improvements — all of which we use heavily in the planned design.
