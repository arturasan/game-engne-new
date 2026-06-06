# Task Completion

- Re-read the active spec before finishing; verify scope, out-of-scope, files-not-to-touch, and acceptance criteria actually match what changed.
- Required local loop for coding tasks: `cmake --workflow --preset check` unless the user explicitly forbids builds/tests.
- For preset-specific verification: run `ctest --preset linux-clang-asan` after building; use `linux-gcc-rel` too for merge-level confidence.
- Run/check public-header dependency canary before PR/merge: `grep -rE 'SDL_|SDL_GPU|glm::|spdlog|ImGui' engine/**/*.hpp` must return zero hits.
- If examples/runtime behavior are involved, inspect `build/logs/last_run.jsonl` and report meaningful log failures.
- Tests added/updated should be focused doctest cases tagged `[fast]` where possible; `[fast]` tests must stay under 100 ms each.
- Renderer/screenshot specs: never hand-edit `tests/refs` PNGs; use the harness update flag when implemented.
- Before PR: update spec metadata (`Status`, `Implementation PR`) and check only acceptance criteria actually satisfied. Leave partial criteria unchecked with a note.
- Commit message must be Conventional Commit style.
- Do not amend or force-push unless explicitly asked. Do not disable pre-commit hooks.
- If blocked after spec/log/ADR checks, stop and report instead of refactoring adjacent code without a spec.
