# Testing strategy

## Layers

| Layer | What it verifies | Tool | Speed budget |
|---|---|---|---|
| Unit (`[fast]`) | Pure logic on small inputs | doctest | < 100 ms per case |
| Unit (`[slow]`) | Heavier logic or I/O | doctest | < 5 s per case |
| Replay | Deterministic simulation hash equals golden | doctest harness | < 2 s per case |
| Screenshot | Rendered output matches reference PNG | doctest + `odiff` | < 5 s per case |
| Integration | Whole-app smoke test of an example | shell script in CI | < 30 s per case |

`cmake --workflow --preset check` runs only `[fast]` tests via the `linux-clang-asan` test preset. Replay and screenshot suites remain part of the strategy and can be run explicitly as dedicated harnesses.

## File layout

```
tests/
├── CMakeLists.txt
├── unit/
│   ├── test_app.cpp          # one source per area; multiple TEST_CASEs per file
│   ├── test_ecs.cpp
│   ├── test_assets.cpp
│   └── ...
├── replay/
│   ├── README.md
│   ├── recordings/           # *.replay — input + seed + frame count
│   └── refs/                 # *.hashes — golden frame hashes
└── refs/
    ├── README.md
    └── sprite_demo_frame_30.png   # golden screenshot PNGs
```

## doctest conventions

- One `TEST_CASE` per behavior, not per function.
- Tag every test: `* doctest::test_suite("fast")` or `"slow"`.
- Use `SUBCASE` for table-driven cases sharing setup.
- Asserts: `CHECK` (continue on fail) for independent checks, `REQUIRE` (abort) for preconditions.
- Never `printf` from a passing test. Use `INFO()` for context that only prints on failure.

```cpp
TEST_CASE("Query iterates over matching archetypes" * doctest::test_suite("fast")) {
    World w;
    auto e1 = w.spawn(Transform{}, Velocity{});
    auto e2 = w.spawn(Transform{});                  // missing Velocity, filtered out
    int hits = 0;
    w.query<Transform, Velocity>().each([&](Entity, auto&, auto&) { ++hits; });
    CHECK(hits == 1);
}
```

## Deterministic replay

A **replay** is `(input_stream, seed, frame_count) → frame_hash_sequence`.

```cpp
struct Replay {
    std::uint64_t seed;
    std::uint32_t frames;
    std::vector<std::variant<KeyEvent, MouseButtonEvent, ...>> inputs;  // timestamped per frame
};
```

The harness:

1. Spins up an `App` with `HeadlessPlugin`, `ReplayPlugin{ recording = r }`, and the example under test.
2. Per frame, feeds the recorded inputs *before* `PreUpdate`.
3. Hashes the state of `(Transform, Velocity, ...other "world" components)` of every entity after `PostUpdate` into a per-frame digest (SHA-256, 32 bytes).
4. Compares against the golden hash file `tests/replay/refs/<name>.hashes`.

```cpp
TEST_CASE("sprite_demo replay" * doctest::test_suite("fast")) {
    auto result = run_replay("sprite_demo_001");
    CHECK(result.matches_golden);
}
```

Updating goldens: `--update-replay-refs` flag on the harness; commit the diff.

**What replays catch:** any non-determinism slipping into the sim — uninitialized memory, unordered iteration, FP-mode changes, accidentally seeded-by-time RNG, parallel system reorder.

**What they don't catch:** rendering output (use screenshots), audio (no equivalent yet — see open questions).

## Screenshot diff

```cpp
TEST_CASE("sprite_demo renders correctly" * doctest::test_suite("slow")) {
    run_example_headless("sprite_demo", { .frames = 30 });
    CHECK(compare_screenshot(
        "build/screenshots/sprite_demo_frame_30.png",
        "tests/refs/sprite_demo_frame_30.png",
        ScreenshotOpts{ .perceptual = true, .max_diff_px = 16 }));
}
```

- Reference PNGs in `tests/refs/`. **Never edit by hand.** Use `--update-refs`.
- Comparison: [odiff](https://github.com/dmtrKovalenko/odiff) (perceptual, fast). Allows N pixels of diff to absorb encoder noise.
- **Pin to software rasterizer.** llvmpipe (Linux) or SwiftShader (cross-platform). Drivers / GPUs differ in FP rounding; goldens captured on one GPU won't match on another.

## Test data and fixtures

- Fixtures live next to the test, not in a shared dir: `tests/unit/fixtures/test_ecs/`.
- Generated test data goes in `build/test-data/<test-name>/`, never committed.
- Replays and golden PNGs are the exception — they're committed and version-controlled.

## CI matrix coverage

| Job | Tests run |
|---|---|
| linux-clang-asan | `[fast]` (PR blocker) |
| linux-gcc-rel | `[fast]` (PR blocker) |

Windows/macOS and nightly jobs are planned to return in M5 (see ADR 0002).

## Logging assertions

The structured JSON log at `build/logs/last_run.jsonl` is a first-class test target. Tests can assert on log content:

```cpp
CHECK(log_contains({ .level = "info", .message_contains = "window opened" }));
```

Used heavily for integration tests where there is no visual artifact.

## What is **not** tested (and why)

- **GPU floating-point identity.** Use perceptual diff, not exact.
- **Wall-clock perf** in unit tests. Use a separate `bench/` directory (M3+) under [nanobench](https://github.com/martinus/nanobench).
- **Third-party library correctness.** Trust SDL3 / SDL3 GPU / spdlog / GLM; assert only that we call them correctly.

## Open questions

- Audio diff testing — needed? Decision deferred; manual listening test for M1.
- Mutation testing on the ECS — interesting but expensive; consider in M6.
- Fuzzing the asset loaders — yes, in M3 (libFuzzer on Linux-Clang only).
