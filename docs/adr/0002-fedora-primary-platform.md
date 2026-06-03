# 0002 — Fedora as the sole supported platform through M4

- Status: accepted
- Date: 2026-06-02
- Supersedes part of: 0001 (compiler floor + multi-platform CI)

## Context

The original plan supported Linux + Windows from day one with a five-job CI matrix (linux-clang-asan, linux-gcc-rel, win-msvc-dbg, win-mingw-rel, win-mingw-dbg). This was based on a future public release that is no longer near-term.

Realities:

- The engine will not be released publicly until at least M5.
- The primary developer's workstation runs Fedora with GCC 16 available via toolbox.
- Cross-platform CI doubles wall-clock time per PR and triples the surface area of toolchain bugs an agent has to debug.
- Hiding "every third-party type" is already an expensive rule. Adding "must build on MinGW UCRT + MSVC + Linux" on top of that is a second tax we don't need yet.

## Decision

Through milestones M0–M4, the engine targets **Fedora 40+ only**.

- **Primary compiler:** GCC 16 (via `toolbox enter` if not the host default).
- **Secondary compiler:** Clang 20 (for ASan/UBSan in CI).
- **Compiler floor:** GCC 16 and Clang 20. Drop GCC 15 and MSVC entirely.
- **C++ standard:** `-std=c++26` where both compilers agree it's safe; `-std=c++23` for files using features one compiler lacks.
- **C++26 feature-test guarding:** still recommended for forward compatibility, but a missing fallback is no longer a PR blocker.
- **CI:** two jobs only — `linux-clang-asan` and `linux-gcc-rel`. Both must be green to merge.
- **Windows presets, bootstrap script, and `docs/setup/windows.md`:** kept in-tree as future reference but marked **unmaintained**.

Multi-platform expansion is **M5**. ADR 0002 is superseded at the start of M5 by a new ADR re-introducing Windows + macOS + Web targets and their compiler floors.

## Consequences

- **Positive:** PR CI time roughly halves. One toolchain to debug. One platform to test screenshot determinism on (llvmpipe). Agents get faster, more reliable feedback.
- **Positive:** We can use C++26 features freely as GCC 16 / Clang 20 implement them — `std::execution`, `<inplace_vector>`, `<hazard_pointer>`, `= delete("reason")`, `constexpr` placement new, etc.
- **Positive:** Removes the "MinGW vs MSVC vs Linux" compatibility burden during the high-churn period of M1–M4.
- **Negative:** Code may accumulate Linux-isms (path separators, filesystem casing, signal handling) that need cleanup at M5. Mitigation: keep `std::filesystem::path` everywhere, never hard-code `/`, run `clang-tidy portability-*` checks.
- **Negative:** Windows users (including future contributors) cannot build until M5. Documented in `docs/setup/windows.md`.

## Alternatives considered

- **Keep the full matrix.** Rejected: pays the cost now for a benefit that's at least a year out.
- **Fedora + Windows MinGW only (no MSVC).** Rejected: MinGW alone catches almost none of the real Windows portability bugs. Either commit to MSVC or drop Windows entirely.
- **Fedora only, but also macOS.** Rejected: same logic as Windows. M5 brings all three back together.

## Triggers to revisit

Reopen this ADR (and write its successor) when **any** of these happen:

1. A second developer joins on a non-Fedora workstation.
2. M4 demo gate (Pong networking) is passing.
3. An external contributor needs to build the engine.
4. A C++26 feature we depend on stalls in GCC and is well-supported in Clang/MSVC.
