# Conventions

- `.clang-format` and `.clang-tidy` are authoritative. Clang-tidy enables bugprone, analyzer, cppcoreguidelines, modernize, performance, portability, readability with project-specific exclusions.
- Naming: files/functions/variables `snake_case`; types/enums/concepts `CamelCase`; namespaces `lower_case`; private data members trailing `_`; macros `UPPER_CASE` only.
- Headers: `.hpp`, `#pragma once`, forward-declare aggressively, include one header per directly used type, external/third-party includes only outside public engine headers unless hidden behind backend/detail implementation.
- Public engine headers must expose only `engine::`/standard types. Third-party deps belong in `*_backend.cpp` or internal detail boundaries that do not leak public ABI.
- Prefer free functions when no state is captured. Member functions should protect object invariants/state.
- Fallible public APIs return `engine::Result<T>` (`std::expected<T, Error>`). Infallible APIs return plain `T`. No exceptions across module/plugin/backend boundaries.
- `Result<T>::value()` is forbidden in engine code outside tests/examples; explicitly branch or propagate.
- Programmer errors/invariants use asserts; runtime failures use `Result` or nullable pointer where documented.
- ECS determinism matters: preserve insertion-deterministic iteration order; do not rely on entity IDs for serialized identity.
- Tests: doctest, one `TEST_CASE` per behavior, tag with `doctest::test_suite("fast")` or `"slow"`, use `SUBCASE` for shared setup, avoid stdout from passing tests.
- Comments should explain non-obvious why/invariants/workarounds only. Do not add comments that restate code or reference current PR/task.
- Conventional commits: `feat`, `fix`, `refactor`, `docs`, `test`, `build`, `ci`, `chore`; scope usually affected `engine/` or `plugins/` area.
- Spec discipline: update spec status/checklist/PR link when opening PR; do not touch files listed in a spec's files-not-to-touch section.
