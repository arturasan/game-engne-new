# 0015 — Environment diagnostics and launch tooling

- Owner: TBD
- Milestone: M1
- Status: draft
- Tracking issue: https://github.com/arturasan/game-engne-new/issues/16
- Implementation PR: TBD
- Merged in: TBD

## Scope and motivation

Spec 0015 owns the diagnostics, launch-mode, diagnostic-bundle, and rendered
success verification layer that builds on implemented spec 0014.

It exists because renderer spec 0005 proved that process exit code `0` is not a
rendering proof: renderer initialization failure can request clean application
exit. It also proved that SDL, Vulkan, Toolbx, Wayland/X11, lavapipe, and NVIDIA
hardware-presentation state must be reported in one reproducible place instead
of reconstructed manually during every failure.

0015 is not an engine runtime feature and does not add runtime dependencies.

## Depends on

- Spec 0014 implemented: developer environment, bootstrap, repo-local vcpkg,
  generated presets, canonical path handling, and normal desktop CLion workflow.
- Spec 0005 implemented: renderer clear-color foundation and
  `examples/clear_color/`.

## Official references

Implementation work must use current official primary documentation:

- SDL video driver enumeration:
  https://wiki.libsdl.org/SDL3/SDL_GetNumVideoDrivers and
  https://wiki.libsdl.org/SDL3/SDL_GetVideoDriver
- SDL active video driver:
  https://wiki.libsdl.org/SDL3/SDL_GetCurrentVideoDriver
- SDL GPU driver enumeration and active GPU driver:
  https://wiki.libsdl.org/SDL3/SDL_GetNumGPUDrivers,
  https://wiki.libsdl.org/SDL3/SDL_GetGPUDriver, and
  https://wiki.libsdl.org/SDL3/SDL_GetGPUDeviceDriver
- SDL GPU device properties:
  https://wiki.libsdl.org/SDL3/SDL_GetGPUDeviceProperties
- Vulkan loader guide:
  https://docs.vulkan.org/guide/latest/loader.html
- Vulkan layers and validation:
  https://docs.vulkan.org/guide/latest/layers.html and
  https://docs.vulkan.org/guide/latest/validation_overview.html
- Vulkan physical-device enumeration:
  https://docs.vulkan.org/refpages/latest/refpages/source/vkEnumeratePhysicalDevices.html
- Vulkan loader driver discovery and ICD manifests:
  https://vulkan.lunarg.com/doc/view/latest/linux/LoaderDriverInterface.html
- GitHub Actions workflow artifacts:
  https://docs.github.com/actions/concepts/workflows-and-actions/workflow-artifacts
- GitHub `upload-artifact` behavior:
  https://github.com/actions/upload-artifact
- CMake Presets manual:
  https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html
- vcpkg manifest mode:
  https://learn.microsoft.com/vcpkg/concepts/manifest-mode

Bevy is not an authority for this tooling task.

## Environment doctor

The implementation must define:

```text
./tools/dev doctor
```

`doctor` is read-only by default. It must print a concise human report and write
versioned JSON to:

```text
build/diagnostics/system.json
```

Required report areas:

- schema version and timestamp;
- host OS, immutable Fedora/Kinoite version, kernel, and architecture;
- desktop session, Wayland display, X11 display, and runtime directory;
- selected development environment and Toolbx image identity from 0014;
- canonical repository path from 0014;
- compiler, CMake, Ninja, mold, and sccache paths and versions;
- vcpkg root, executable, baseline, and installed SDL feature set;
- CMake preset availability and last configure health;
- SDL version;
- compiled SDL video drivers;
- compiled SDL GPU drivers;
- active SDL video/GPU driver where available;
- Vulkan loader version;
- available ICD files;
- enumerated physical devices;
- software lavapipe availability;
- validation-layer availability;
- `/dev/dri` visibility;
- relevant NVIDIA device/driver visibility without exposing secrets;
- Git branch, HEAD, remote tracking, and working-tree state;
- local known-limitations data for issues #13 and #14 where practical.

Doctor must not require GitHub authentication or network access. Querying live
GitHub issue state is optional and must never turn an otherwise valid local
doctor run into failure. Offline output must remain complete and useful.

JSON privacy:

- do not dump the complete environment;
- include only selected non-secret environment values such as `PATH`, display
  variables, `XDG_RUNTIME_DIR`, `VK_DRIVER_FILES`, `VK_ICD_FILENAMES`,
  `SDL_VIDEODRIVER`, `SDL_GPU_DRIVER`, and vcpkg/cache paths after redaction
  review;
- never include tokens, SSH keys, cookies, complete home-directory listings,
  browser data, JetBrains account files, arbitrary `.idea` state, or arbitrary
  dotfiles.

Stable severity levels:

- `pass`;
- `warning`;
- `failure`;
- `optional/unavailable`.

Exit codes:

- `0`: no failures;
- `1`: one or more required checks failed;
- `2`: invalid command-line usage;
- `3`: doctor itself could not complete or could not write JSON;
- `4`: privacy guard refused to write a bundle or report.

## Diagnostic bundle

The implementation must define:

```text
./tools/dev bundle
```

It must create a timestamped, privacy-reviewed bundle under:

```text
build/diagnostics/<timestamp>/
```

Include:

- `system.json`;
- human-readable doctor report;
- exact command lines;
- selected non-secret environment values;
- CMake configure summary;
- engine structured log;
- renderer/backend summary;
- smoke-test result.

Do not include tokens, SSH keys, complete environment dumps, home-directory
contents, browser data, JetBrains account files, arbitrary `.idea` state, or
arbitrary dotfiles.

The bundle command must run `doctor` first unless a fresh
`build/diagnostics/system.json` is supplied explicitly.

## Standard run commands

The implementation must define one coherent CLI instead of unrelated magic
scripts:

```text
./tools/dev run --mode headless clear_color
./tools/dev run --mode windowed-software clear_color
./tools/dev run --mode windowed-hardware clear_color
./tools/dev run --mode validation clear_color
```

The command must remain discoverable through:

```text
./tools/dev run --help
```

Each run must:

- select the correct CMake preset and build configuration;
- build the target if needed;
- set only required environment variables;
- record the exact command;
- capture logs;
- produce a clear pass/fail result;
- identify selected SDL and Vulkan backends;
- identify renderer backend and selected device where available;
- write results under `build/diagnostics/` or `build/test-output/`;
- never treat process exit code `0` alone as rendered success.

Mode requirements:

- `headless`: uses headless renderer behavior and deterministic software Vulkan
  settings where applicable;
- `windowed-software`: opens a real window with lavapipe/software Vulkan to
  prove compositor/window integration independently of hardware GPU interop;
- `windowed-hardware`: attempts default hardware presentation and reports issue
  #14-class failures precisely;
- `validation`: enables Vulkan/SDL validation diagnostics where available and
  captures them in the diagnostic bundle.

## Explicit rendering-success contract

The implementation must define a stable machine-readable success marker emitted
only after at least one frame is successfully rendered and submitted. Example:

```text
ENGINE_SMOKE_RESULT status=rendered frame=1 mode=windowed-software backend=vulkan device="llvmpipe ..."
```

The exact field list may differ, but the marker must:

- be stable and parseable;
- distinguish renderer initialization failure from successful rendering;
- identify headless versus windowed mode;
- identify software versus hardware device;
- identify the SDL GPU backend and active video driver where available;
- be emitted only after actual renderer success;
- be consumed by run wrappers and CI;
- include failure markers for initialization failure, frame skip, backend
  absence, and validation failure.

The implementation should avoid adding a public engine API solely for this
marker if an example-level or structured-log implementation is sufficient.

## Issue #14 diagnostic support

0015 must make hardware Vulkan presentation failures precise and reproducible,
but it must not claim to fix proprietary driver or host/container GPU interop.

Required behavior:

- detect `/dev/dri` visibility;
- report Wayland and X11 display variables;
- report Vulkan ICD files and physical devices;
- distinguish lavapipe/software success from hardware success;
- report NVIDIA device/driver visibility without secrets;
- preserve a clean `optional/unavailable` result when hardware GPU access is not
  present;
- never claim `windowed-hardware` success without the rendered-frame marker.

## Tests and CI

Implementation must define:

- fast tests for doctor output-schema behavior;
- fast tests for command-line parsing and run-mode selection;
- JSON structural validation without adding a large dependency;
- privacy tests proving forbidden environment keys and obvious secret patterns
  are redacted;
- negative tests proving missing tools, missing SDL drivers, missing Vulkan ICDs,
  and invalid configurations produce actionable errors;
- CI smoke for `doctor` that does not require GitHub authentication or network;
- software Vulkan smoke validation;
- run-wrapper tests proving process exit `0` without rendered-frame marker fails;
- preservation of the existing 52 fast engine tests and slow renderer suite.

CI must not require an NVIDIA GPU. Hardware-windowed mode may report
`optional/unavailable` in CI.

## Files allowed

The 0015 implementation PR is expected to stay within this bounded surface:

- `tools/dev/**`;
- tooling JSON-schema or schema-test files;
- `.gitignore`;
- narrow CI changes for diagnostics and software Vulkan smoke;
- narrow example/log success-marker changes;
- tooling tests;
- `specs/0015-diagnostics-launch-tooling.md`;
- roadmap/development-plan/example-map metadata.

This does not grant permission to redesign engine runtime systems.

## Out of scope

0015 explicitly excludes:

- bootstrap environment creation owned by 0014;
- normal desktop CLion integration owned by 0014;
- Tracy dependency or instrumentation;
- RenderDoc automated capture integration;
- Nsight Graphics;
- Nsight Systems;
- AMD uProf;
- VTune;
- Radeon GPU Profiler;
- GFXReconstruct automation;
- custom RHI design or selection;
- renderer architecture changes;
- Windows/macOS developer environments;
- automatic installation or modification of proprietary GPU drivers;
- editor plugins unrelated to build/run/debug;
- broad CI redesign.

## Future observability path

Later work should investigate:

- RenderDoc;
- Tracy, requiring an ADR before adding a dependency;
- Nsight Graphics and Nsight Systems for NVIDIA;
- AMD uProf and Linux `perf` for CPU work;
- heaptrack;
- Radeon tools for future AMD GPU machines;
- GFXReconstruct;
- RHI observability requirements such as debug labels, timestamps, breadcrumbs,
  and device-fault reporting.

Do not assign new spec numbers for these follow-ups in this PR.

## Acceptance criteria

- [ ] `./tools/dev doctor` writes a concise human report.
- [ ] `./tools/dev doctor` writes `build/diagnostics/system.json`.
- [ ] `system.json` includes schema version, timestamp, selected environment,
      canonical path, tool versions, vcpkg state, SDL state, Vulkan state, GPU
      visibility, Git state, and local known limitations.
- [ ] Doctor is useful offline and does not require GitHub authentication.
- [ ] Doctor never dumps complete environment state or secrets.
- [ ] `./tools/dev bundle` creates a timestamped diagnostic bundle.
- [ ] Generated bundle contains no secrets or home-directory dumps.
- [ ] `./tools/dev run --mode headless clear_color` passes in the supported
      environment.
- [ ] `./tools/dev run --mode windowed-software clear_color` passes with
      lavapipe.
- [ ] `./tools/dev run --mode windowed-hardware clear_color` reports precise
      diagnostics and does not claim success on clean initialization failure.
- [ ] `./tools/dev run --mode validation clear_color` captures validation output
      where available.
- [ ] Exact command lines are recorded for doctor, bundle, and run commands.
- [ ] Renderer/backend/device reporting is included in run output.
- [ ] Explicit rendered-frame marker is emitted and verified.
- [ ] Process exit code `0` without the rendered-frame marker is treated as
      failure for smoke success.
- [ ] CI validates tooling schema behavior without an NVIDIA GPU.
- [ ] CI runs software Vulkan smoke.
- [ ] Existing 52 fast engine tests remain green.
- [ ] Existing slow renderer validation remains green where supported.

## Implementation sequencing

0015 is one bounded implementation PR after 0014 is implemented. If the
diagnostics system grows beyond this scope, split only after identifying a
concrete boundary and keep the initial implementation focused on doctor,
bundle, explicit run modes, and rendered-frame verification.
