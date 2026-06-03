# 0009 — Deterministic replay harness

- Owner: TBD
- Milestone: M1
- Status: draft
- Tracking issue: TBD
- Implementation PR: TBD
- Merged in: TBD

## Scope

Build the deterministic replay infrastructure used by `[fast]` tests. A replay is `(input_stream, seed, frame_count) → frame_hash_sequence`. Compare against a golden hashes file checked into `tests/replay/refs/`.

## Acceptance criteria

- [ ] `engine::Replay { uint64 seed; uint32 frames; vector<TimedInputEvent> inputs; }`.
- [ ] `ReplayPlugin { Replay recording; }` registers itself as an input source (replaces real platform input when present).
- [ ] `HeadlessPlugin` opts the app out of windowing + presenting; combine with `RenderPlugin` headless mode (spec 0005).
- [ ] `HashFramePlugin` hashes the state of `(Transform, Velocity, ...whitelist of components)` of every entity after `PostUpdate` and writes the per-frame SHA-256 digest to a buffer.
- [ ] `run_replay(const std::string& name) -> ReplayResult { matches_golden, mismatch_frame, actual_hashes }`.
- [ ] CLI flag `--update-replay-refs` on the test binary rewrites the golden file.
- [ ] `Replay` is serializable to and from a small binary format in `tests/replay/recordings/<name>.replay`.
- [ ] A `[fast]` doctest in `tests/unit/test_replay.cpp` validates: same recording + same code → identical hashes; flipped seed → different hashes; missing component in whitelist → still deterministic.
- [ ] One end-to-end replay (`tests/replay/recordings/sprite_demo_001.replay`) of `sprite_demo` runs in CI and passes.

## Out of scope

- Audio determinism (separate, see open questions in `07-testing.md`).
- Render output determinism (that's screenshot tests, spec 0010).
- Replay editing UI.
- Capturing replays from real gameplay sessions — manual hand-authored for M1.

## Files not to touch

- `engine/render*` (consumer; headless mode landed in 0005).
- `plugins/sprite/*` (used as a test subject only).

## Notes for the implementing agent

- Read `docs/architecture/07-testing.md` — the "Deterministic replay" section is the contract.
- Hash: SHA-256 from `<openssl>` is too heavyweight. Use BLAKE3 (vcpkg `blake3`) or hand-rolled SHA-256 (`picosha2`). BLAKE3 preferred.
- Whitelist of hashed components: pass via template list to `HashFramePlugin<Transform, Velocity, ...>`. Compile-time, fast, no reflection needed.
- Iteration order over entities **must** be deterministic. The ECS (spec 0001) already promises archetype order; assert this in the hash system to catch regressions.
- Replay binary format: little-endian, prefixed by magic `ENGRPLY\0` and version byte. Keep tight; replays are committed.
