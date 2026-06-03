# 0000 — Phase-0 bring-up on Fedora

- Owner: TBD (first agent on the new workstation)
- Status: merged
- Tracking issue: TBD

## Scope

The Phase-0 scaffold (`AGENTS.md`, `CMakeLists.txt`, `CMakePresets.json`, `vcpkg.json`, `engine/core/*`, `tests/unit/test_app.cpp`, `examples/hello_window/`, CI workflow, pre-commit config) was authored without ever being compiled. This historical spec tracked the first end-to-end build on a real Fedora 40+ workstation with GCC 16 / Clang 20.

This was **not** feature work. The goal was: scaffold builds, tests pass, CI is green. Nothing more.

## Acceptance criteria

- [x] `docs/setup/fedora.md` followed end-to-end on a fresh clone produces a working environment.
- [x] `cmake --preset linux-clang-asan` configures with zero warnings (or with warnings tracked in this spec).
- [x] `cmake --build --preset linux-clang-asan` builds cleanly.
- [x] `cmake --preset linux-gcc-rel` configures + builds cleanly.
- [x] `ctest --preset linux-clang-asan` passes (all 3 `test_app.cpp` cases green).
- [x] `cmake --workflow --preset check` finishes in under 60s cold, under 10s incremental.
- [x] `./build/linux-clang-asan/Debug/examples/hello_window/hello_window` prints `frame 0` through `frame 4` and exits 0.
- [x] `pre-commit run --all-files` passes on a fresh clone (after `pre-commit install`).
- [x] GitHub Actions CI green on a no-op PR for both `linux-clang-asan` and `linux-gcc-rel` jobs.
- [x] `grep -rE 'SDL_|SDL_GPU|glm::|spdlog' engine/**/*.hpp` returns zero hits (the abstraction-rule canary).

## Allowed changes

- Fix CMake typos, missing `target_link_libraries`, missing includes.
- Fix `vcpkg.json` version pins that vcpkg refuses.
- Fix preset names / paths that don't actually exist.
- Add the `cmake --workflow --preset check` workflow preset if missing or broken.
- Fix CI YAML errors (dnf package names, action versions, container image tags).
- Update `.pre-commit-config.yaml` hook versions to ones that actually install.
- Add or fix `.gitignore` entries that prevent a clean tree post-build.

## Out of scope

- Any new feature, file, or directory not already present in the M0 scaffold.
- Implementation of spec 0001 (ECS) or any other numbered spec.
- Adding new vcpkg dependencies (anything beyond doctest/spdlog/glm/sdl3).
- Refactoring the existing engine core code "while I'm here".
- Editing `AGENTS.md`, architecture docs, or specs unless they describe behavior the bring-up proved wrong.

## Files not to touch

- All `specs/000[1-9]*.md` and `specs/001[0-2]*.md` — they describe future work.
- All `docs/architecture/*.md` — architecture is fixed; if reality disagrees with arch docs, file a follow-up issue rather than editing.

## Notes for the implementing agent

- Expect breakage. Nothing in this repo has ever compiled. Read this spec, read `docs/setup/fedora.md`, then start building.
- When something fails, the fix should be the smallest possible change. If you find yourself rewriting a file, stop — file an issue and ask for guidance instead.
- For every fix, commit with `build:` or `fix:` (Conventional Commits). One concern per commit; this PR will be reviewed line-by-line.
- The first useful inner loop is `cmake --workflow --preset check`. If it doesn't exist or doesn't work, fix that first — every later spec depends on it.
- If a Fedora package is missing or misnamed in `docs/setup/fedora.md` or the CI workflow, update **both** in the same commit.
- If GCC 16 or Clang 20 are not available on your host, use `toolbox create --image registry.fedoraproject.org/fedora:rawhide engine` per the setup doc. Do not lower the compiler floor; ADR 0002 is firm.

## What "done" looks like

A PR titled `build: bring up Phase-0 on Fedora` with:

- Green CI on both jobs.
- A short `## Findings` section in the PR body listing what was broken and how it was fixed.
- Spec 0001 unblocked and ready to start.
