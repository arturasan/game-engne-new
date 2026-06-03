# 0003 — Structured JSON logging

- Owner: TBD
- Status: draft
- Tracking issue: TBD

## Scope

Wrap spdlog behind `engine::log` in `engine/core/log.hpp`. Default sinks: pretty colored stderr (for humans) and JSON-lines to `build/logs/last_run.jsonl` (for agents and tests).

## Acceptance criteria

- [ ] `engine::log::trace/debug/info/warn/error/critical(fmt, args...)` use `std::format` syntax.
- [ ] JSON sink writes one object per line: `{ts, level, target, message, thread, source_location}`.
- [ ] `LogPlugin::build(App&)` installs both sinks. Sink configuration via `LogConfig { level, json_path, color }`.
- [ ] `last_run.jsonl` is truncated at process start, appended during run, flushed on `App::run()` exit.
- [ ] `log_contains({level, message_contains})` test helper in `tests/support/log_assert.hpp` parses the JSON and returns `bool`.
- [ ] Public headers include no spdlog. Grep test: `grep -r 'spdlog' engine/**/*.hpp` returns zero hits.
- [ ] `engine::log::set_level(LogLevel)` works at runtime.
- [ ] Unit tests in `tests/unit/test_log.cpp` tagged `[fast]`: format placeholders, level filtering, JSON well-formedness, `log_contains` helper.

## Out of scope

- Network / remote log sinks.
- Per-target log levels.
- Async / lock-free logging (M3+ if profiling shows it).
- Log rotation / multi-file.

## Files not to touch

- `engine/platform/*`, `engine/ecs/*`, `engine/render*`.

## Notes for the implementing agent

- Use `spdlog::async_logger` only if needed; default sync is fine for M1.
- The JSON format is a **test contract**. Document the schema in `docs/architecture/07-testing.md` under "Logging assertions" if you change it.
- `source_location` comes from `std::source_location::current()` captured at the call site via a macro `ENGINE_LOG_HERE` or a `consteval` wrapper. Prefer the wrapper; macros only if the compiler can't inline.
- Path resolution: `build/logs/last_run.jsonl` is relative to CWD. Document this.
