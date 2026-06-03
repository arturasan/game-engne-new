# 0010 — Screenshot diff harness

- Owner: TBD
- Milestone: M1
- Status: draft
- Tracking issue: TBD
- Implementation PR: TBD
- Merged in: TBD

## Scope

Build the screenshot-diff testing infrastructure. Render an example headlessly for N frames, capture frame N's color buffer to PNG, compare against a golden in `tests/refs/` using perceptual diff via `odiff`.

## Acceptance criteria

- [ ] `run_example_headless(name, { .frames = N })` test helper runs an example to frame N and returns the path to the captured PNG.
- [ ] `compare_screenshot(actual_path, ref_path, ScreenshotOpts { .perceptual, .max_diff_px })` returns `bool` and writes a diff PNG next to `actual_path` on mismatch.
- [ ] `odiff` is invoked as a subprocess; the binary is downloaded by CMake on first configure to `build/_tools/odiff` (cached by sccache or via `FetchContent`).
- [ ] `--update-refs` test CLI flag copies captured PNGs over the goldens.
- [ ] CI matrix runs screenshot tests **only** on jobs pinned to the software rasterizer (SwiftShader for Vulkan).
- [ ] `tests/refs/README.md` documents: how to regenerate, why we pin to software rasterizer, why diff PNGs are gitignored.
- [ ] Screenshot test for `examples/clear_color/` (capture frame 5, compare against ref). Tagged `[slow]`.
- [ ] Screenshot test for `examples/sprite_demo/` from spec 0008. Tagged `[slow]`.

## Out of scope

- Hardware-GPU screenshot tests (still flaky in 2026; revisit M4).
- Continuous capture / video diff.
- Diff visualization UI.
- Auto-regenerating goldens on CI (always requires explicit `--update-refs` + commit).

## Files not to touch

- `tests/refs/*.png` — generated only; never hand-edited.
- `engine/render*` — consumer of, not editor.

## Notes for the implementing agent

- Read `docs/architecture/07-testing.md` "Screenshot diff" section.
- `odiff` releases at https://github.com/dmtrKovalenko/odiff/releases. Pin to a specific version in CMake (don't track `latest`).
- SwiftShader install: provided by Vulkan SDK on Linux + Windows. Document the env vars (`VK_ICD_FILENAMES`, `VK_LAYER_PATH`) in `docs/setup/`.
- Diff PNG output goes to `build/test-output/<test-name>_diff.png`. Add to `.gitignore`.
- Pixel tolerance: start with `max_diff_px = 16` per test; tune down per-test as goldens stabilize.
