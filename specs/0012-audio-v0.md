# 0012 — Audio v0

- Owner: TBD
- Milestone: M1
- Status: draft
- Tracking issue: TBD
- Implementation PR: TBD
- Merged in: TBD

## Scope

Wrap miniaudio behind `engine::audio` in `engine/audio/`. Ship enough to load + play one-shot sounds and looped music. Public headers must never mention `ma_*` types.

## Acceptance criteria

- [ ] `engine::AudioSource` asset: decoded PCM + sample rate + channel count. Loader supports WAV + OGG Vorbis via miniaudio's decoders.
- [ ] `engine::AudioMixer` resource exposing `play_one_shot(Handle<AudioSource>, AudioOpts)` and `play_looped(...) -> AudioInstanceId`.
- [ ] `AudioOpts { volume, pitch, pan }`.
- [ ] `AudioInstance` controls: `stop(id)`, `set_volume(id, f)`, `set_pitch(id, f)`.
- [ ] `AudioPlugin::build(App&)` initializes the device on a background thread (miniaudio's callback) and inserts the `AudioMixer` resource.
- [ ] Mixer runs in miniaudio's audio thread; the app-facing API is lock-free for `play_*` (SPSC ring buffer of pending commands) and uses a short critical section only for instance state queries.
- [ ] `ENGINE_HEADLESS=1` or `ENGINE_NO_AUDIO=1` swaps in a null audio backend (no device, commands accepted and discarded). Required for CI.
- [ ] Grep test: `grep -r 'ma_\|miniaudio' engine/audio/*.hpp` returns zero hits.
- [ ] Unit test in `tests/unit/test_audio.cpp` tagged `[fast]`: null backend accepts play commands, instance IDs are unique, stop is idempotent.
- [ ] `examples/audio_demo/` plays a 1-second sine WAV on spacebar press.

## Out of scope

- 3D spatial audio (later milestone).
- Effects (reverb, EQ, filters).
- Music crossfading / playlists.
- MP3, FLAC, Opus (WAV + OGG covers M1).
- Audio streaming for large files — fully decoded into memory for v0.
- Replay-determinism for audio (open question in `07-testing.md`).

## Files not to touch

- `engine/render*`, `engine/platform/*`, `plugins/*`.

## Notes for the implementing agent

- miniaudio is a single-header library. Define `MINIAUDIO_IMPLEMENTATION` in **exactly one** `.cpp` — `engine/audio/detail/miniaudio_backend.cpp`.
- Audio threads are externally owned (miniaudio creates them). Never touch ECS / `World` from the audio callback. Communication is via SPSC ring buffers only.
- `AudioInstanceId` = `{uint32 slot, uint32 gen}`. Free slots reused with bumped gen — same pattern as `Entity` (spec 0001) and `Handle<T>` (spec 0007). Consider extracting a `GenerationalId<Tag>` helper.
- Volume / pitch curves: linear volume for v0 is fine; log scaling is a polish item for later.
- Fixture WAVs in `tests/unit/fixtures/test_audio/`. Keep ≤ 100 KB each.
