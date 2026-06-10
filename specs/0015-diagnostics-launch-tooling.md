# 0015 — Environment diagnostics and launch tooling

- Owner: TBD
- Milestone: M1
- Status: draft
- Tracking issue: https://github.com/arturasan/game-engne-new/issues/16
- Implementation PR: TBD
- Merged in: TBD

## Scope and motivation

Spec 0015 owns the core diagnostics, explicit run/smoke tooling,
diagnostic-bundle, renderer/device reporting, and rendered-success
verification layer that builds on implemented spec 0014.

It exists because renderer spec 0005 proved that process exit code `0` is not a
rendering proof: renderer initialization failure can request clean application
exit. It also proved that SDL, Vulkan, Wayland/X11, lavapipe, and NVIDIA
hardware-presentation state must be reported in one reproducible place instead
of reconstructed manually during every failure.

Regular Fedora host-native development is the primary path. Tracked CMake
presets are authoritative. `./tools/dev bootstrap` is only an optional
first-clone helper. Repo-local vcpkg and the root `compile_commands.json`
symlink are implemented by 0014. CLion starts normally without a wrapper.
Toolbx is optional and outside the daily workflow.

RTX 5090 native Wayland presentation now works on the primary Fedora host. The
old Toolbx dmabuf/swapchain failure remains useful historical/container
diagnostic evidence, but 0015 is not a request to repair host-native
presentation from scratch.

0015 is not an engine runtime feature and does not add runtime dependencies.

## Depends on

- Spec 0014 implemented: regular Fedora host-native developer environment,
  optional bootstrap helper, repo-local vcpkg, tracked CMake presets, root
  `compile_commands.json`, and normal desktop CLion workflow.
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
./tools/dev doctor --probe-renderer
```

`doctor` is operationally read-only by default except for its own diagnostic
output. It must print a concise human report and atomically create or replace
versioned JSON at:

```text
build/diagnostics/system.json
```

Required report areas:

- schema version and timestamp;
- host OS, Fedora version/variant, kernel, and architecture;
- desktop session, Wayland display, X11 display, and runtime directory;
- regular host-native development state, with Toolbx/container identity only
  when detected;
- canonical repository path;
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

Default `doctor` may invoke bounded host queries such as:

- Git;
- CMake preset listing;
- compiler version commands;
- `pkg-config`;
- `vulkaninfo`;
- `nvidia-smi` when present;
- environment-variable allowlist checks;
- build-artifact and compile-database checks.

Default `doctor` must not:

- configure;
- build;
- open a window;
- mutate generated state except for its own `build/diagnostics/system.json`
  output and a temporary sibling used for atomic replacement;
- install packages;
- use `sudo`;
- scan the home directory;
- dump the complete environment;
- perform a substantial GPU workload.

Default `doctor` must not modify:

- CMake configure/build trees;
- caches;
- vcpkg;
- bootstrap state;
- pre-commit hooks;
- `compile_commands.json`;
- IDE files;
- source files;
- shell or desktop configuration.

If diagnostic generation fails, no valid-looking replacement `system.json` may
remain.

`doctor --probe-renderer` may run an existing built headless renderer probe.
The probe must:

- use a fixed timeout;
- not build automatically;
- not open a visible window;
- report `unavailable` when the binary is absent;
- record command, exit status, backend/device data, and marker result.

Doctor must not require GitHub authentication or network access. Querying live
GitHub issue state is optional and must never turn an otherwise valid local
doctor run into failure. Offline output must remain complete and useful.

### `system.json` schema

`system.json` schema version 1 is normative. All top-level sections are
required:

```text
schema_version
generated_at
overall_status
host
repository
toolchain
build
graphics
optional_capabilities
checks
redaction
```

Schema version 1 fields:

| Path | Required | Type | Contract |
| --- | --- | --- | --- |
| `schema_version` | yes | integer | Exactly `1`. |
| `generated_at` | yes | string | UTC RFC3339 format exactly `YYYY-MM-DDTHH:MM:SSZ`; no local time and no timezone offset. |
| `overall_status` | yes | enum | `pass`, `warning`, or `fail`. |
| `host.os_id` | yes | string | Fedora/Linux OS identifier. |
| `host.os_version_id` | yes | string | Fedora version identifier. |
| `host.kernel` | yes | string | Kernel release. |
| `host.architecture` | yes | string | Host architecture. |
| `host.session_type` | yes | string | Session type such as `wayland`, `x11`, `tty`, or `unknown`. |
| `host.desktop` | no | string | Desktop environment, omitted when unavailable. |
| `host.fedora_variant` | no | string | Fedora variant, omitted when unavailable. |
| `host.display_server` | no | string | Detected display server, omitted when unavailable. |
| `repository.root` | yes | string | Always serialized as `$REPO`. |
| `repository.commit` | yes | string | Git commit hash. |
| `repository.dirty` | yes | boolean | Working-tree dirty state. |
| `repository.branch` | no | string | Current branch; detached HEAD omits this field. |
| `toolchain.cmake` | yes | object | Required tool object. |
| `toolchain.ninja` | yes | object | Required tool object. |
| `toolchain.cxx_compiler` | yes | object | Required tool object. |
| `toolchain.clangd` | yes | object | Required tool object. |
| `toolchain.*.path` | yes | string | Normalized executable path. |
| `toolchain.*.version` | yes | string | Tool version or empty string only when unavailable. |
| `toolchain.*.status` | yes | enum | `available` or `unavailable`. |
| `build.tracked_presets` | yes | array | Discovered tracked preset names. |
| `build.vcpkg.root` | yes | string | Normalized through `$REPO` when repo-local. |
| `build.vcpkg.revision` | yes | string | Checked-out vcpkg revision or empty string when unavailable. |
| `build.vcpkg.toolchain_file` | yes | string | Normalized toolchain path. |
| `build.vcpkg.status` | yes | enum | `available` or `unavailable`. |
| `build.compile_commands.path` | yes | string | Normalized symlink path. |
| `build.compile_commands.target` | yes | string | Normalized symlink target. |
| `build.compile_commands.status` | yes | enum | `available` or `unavailable`. |
| `graphics.vulkan_loader.version` | yes | string | Vulkan loader version or empty string when unavailable. |
| `graphics.vulkan_loader.status` | yes | enum | `available` or `unavailable`. |
| `graphics.devices` | yes | array | Vulkan physical devices; may be empty. |
| `graphics.devices[].name` | yes | string | Device name. |
| `graphics.devices[].type` | yes | string | Device type such as `cpu`, `integrated_gpu`, `discrete_gpu`, or `unknown`. |
| `graphics.devices[].vendor` | yes | string | Vendor name or numeric vendor rendered without machine-unique IDs. |
| `graphics.devices[].driver_name` | yes | string | Driver name. |
| `graphics.devices[].driver_version` | yes | string | Driver version. |
| `graphics.devices[].api_version` | yes | string | Vulkan API version. |
| `graphics.session` | yes | object | Detected window-system capability without opening a window. |
| `optional_capabilities.lavapipe` | yes | object | Capability object. |
| `optional_capabilities.vulkan_validation` | yes | object | Capability object. |
| `optional_capabilities.graphical_session` | yes | object | Capability object. |
| `optional_capabilities.hardware_vulkan_device` | yes | object | Capability object. |
| `optional_capabilities.renderer_probe` | yes | object | Capability object. |
| `optional_capabilities.*.status` | yes | enum | `available`, `unavailable`, or `not_probed`. |
| `optional_capabilities.*.summary` | yes | string | Short human-readable state. |
| `checks[].id` | yes | string | Stable check ID. |
| `checks[].status` | yes | enum | `pass`, `warning`, `fail`, `unavailable`, or `skipped`. |
| `checks[].severity` | yes | enum | `info`, `warning`, or `error`. |
| `checks[].summary` | yes | string | Short human-readable result. |
| `checks[].observed` | no | string | Observed value, omitted when unavailable. |
| `checks[].expected` | no | string | Expected value, omitted when unavailable. |
| `checks[].remediation` | no | string | Actionable fix, omitted when no remediation applies. |
| `redaction.policy_version` | yes | integer | Exactly `1`. |
| `redaction.normalized_tokens` | yes | array | Token substitutions applied. |
| `redaction.removed_categories` | yes | array | Prohibited categories excluded. |
| `redaction.warnings` | yes | array | Redaction warnings; may be empty. |

Optional or unavailable values must be omitted rather than represented as
`null`, except required string fields may use an empty string only where the
field table explicitly allows it.

`host` must not record hostname, username, UID, machine ID, serial numbers, or
complete environment state. `repository` must not serialize remote URLs
containing credentials. `graphics.devices` must not record UUIDs, PCI bus IDs,
serial numbers, or other stable machine identifiers. Optional renderer-probe
data is omitted unless `doctor --probe-renderer` ran.

Schema version 1 `optional_capabilities` is limited to capabilities directly
required by doctor and the three run modes:

- `lavapipe`;
- `vulkan_validation`;
- `graphical_session`;
- `hardware_vulkan_device`;
- `renderer_probe`.

Do not add RenderDoc, Tracy, Nsight, VTune, `perf`, heaptrack, GDB, or LLDB
detection to 0015.

Stable schema-v1 check IDs:

```text
host.os
host.session
repository.state
toolchain.cmake
toolchain.ninja
toolchain.cxx
toolchain.clangd
build.presets
build.vcpkg
build.compile_commands
graphics.vulkan_loader
graphics.vulkan_devices
graphics.graphical_session
graphics.lavapipe
graphics.validation_layers
graphics.renderer_probe
privacy.redaction
```

Future checks may be added within schema version 1, but existing IDs and
meanings must not change.

`overall_status` derivation:

1. `fail` when any check has `status = fail` and `severity = error`.
2. Otherwise `warning` when any check has `status = warning`, `status = fail`
   with `severity = warning`, or `status = unavailable` with
   `severity = warning`.
3. Otherwise `pass`.

`unavailable` or `skipped` checks with `severity = info` do not elevate overall
status. Required capability failures must use severity `error`. Unavailable
optional capabilities normally use severity `info`.

Complete example:

```json
{
  "schema_version": 1,
  "generated_at": "2026-06-10T12:34:56Z",
  "overall_status": "warning",
  "host": {
    "os_id": "fedora",
    "os_version_id": "40",
    "kernel": "6.10.0-0.fc40.x86_64",
    "architecture": "x86_64",
    "session_type": "wayland",
    "desktop": "KDE",
    "fedora_variant": "workstation",
    "display_server": "wayland"
  },
  "repository": {
    "root": "$REPO",
    "commit": "666ea8300ac45279b716486d51023677d6d16a0c",
    "dirty": false,
    "branch": "docs/review-diagnostics-launch-tooling-0015"
  },
  "toolchain": {
    "cmake": {
      "path": "/usr/bin/cmake",
      "version": "3.30.0",
      "status": "available"
    },
    "ninja": {
      "path": "/usr/bin/ninja",
      "version": "1.12.1",
      "status": "available"
    },
    "cxx_compiler": {
      "path": "/usr/bin/clang++",
      "version": "20.0.0",
      "status": "available"
    },
    "clangd": {
      "path": "/usr/bin/clangd",
      "version": "20.0.0",
      "status": "available"
    }
  },
  "build": {
    "tracked_presets": [
      "linux-clang-asan",
      "linux-gcc-rel",
      "check"
    ],
    "vcpkg": {
      "root": "$REPO/.cache/dev/vcpkg",
      "revision": "059d760472984042e1b4db0d40efd935a1adcbc9",
      "toolchain_file": "$REPO/.cache/dev/vcpkg/scripts/buildsystems/vcpkg.cmake",
      "status": "available"
    },
    "compile_commands": {
      "path": "$REPO/compile_commands.json",
      "target": "$REPO/build/linux-clang-asan/compile_commands.json",
      "status": "available"
    }
  },
  "graphics": {
    "vulkan_loader": {
      "version": "1.3.290",
      "status": "available"
    },
    "devices": [
      {
        "name": "llvmpipe",
        "type": "cpu",
        "vendor": "Mesa",
        "driver_name": "llvmpipe",
        "driver_version": "24.1.0",
        "api_version": "1.3"
      }
    ],
    "session": {
      "wayland": "available",
      "x11": "unavailable",
      "visible_window_probe": "not_run"
    }
  },
  "optional_capabilities": {
    "lavapipe": {
      "status": "available",
      "summary": "software Vulkan device detected"
    },
    "vulkan_validation": {
      "status": "unavailable",
      "summary": "validation layer package not detected"
    },
    "graphical_session": {
      "status": "available",
      "summary": "Wayland session variables detected"
    },
    "hardware_vulkan_device": {
      "status": "unavailable",
      "summary": "no discrete hardware Vulkan device detected"
    },
    "renderer_probe": {
      "status": "not_probed",
      "summary": "doctor --probe-renderer was not requested"
    }
  },
  "checks": [
    {
      "id": "host.os",
      "status": "pass",
      "severity": "info",
      "summary": "Fedora host detected",
      "observed": "fedora 40",
      "expected": "Fedora host"
    },
    {
      "id": "graphics.validation_layers",
      "status": "unavailable",
      "severity": "info",
      "summary": "Vulkan validation layers are not installed",
      "remediation": "Install the Fedora Vulkan validation layers package before using --validation."
    }
  ],
  "redaction": {
    "policy_version": 1,
    "normalized_tokens": [
      "$REPO",
      "$HOME",
      "$XDG_RUNTIME_DIR"
    ],
    "removed_categories": [
      "credentials",
      "complete_environment",
      "shell_history",
      "home_directory_listing"
    ],
    "warnings": []
  }
}
```

JSON privacy:

- do not dump the complete environment;
- include only selected non-secret environment values such as `PATH`, display
  variables, `XDG_RUNTIME_DIR`, `VK_DRIVER_FILES`, `VK_ICD_FILENAMES`,
  `SDL_VIDEODRIVER`, `SDL_GPU_DRIVER`, and vcpkg/cache paths after redaction
  review;
- never include tokens, SSH keys, cookies, complete home-directory listings,
  browser data, JetBrains account files, arbitrary `.idea` state, or arbitrary
  dotfiles;
- never include shell history, arbitrary home paths, full process environment,
  or raw command output containing unreviewed secrets.

Redaction metadata must include:

- redaction policy version;
- whether paths were normalized;
- fields removed;
- warnings about values requiring manual review.

Path normalization must apply these replacements before JSON or bundle output,
using longest-prefix-first matching:

```text
canonical repository root -> $REPO
user home directory        -> $HOME
XDG_RUNTIME_DIR            -> $XDG_RUNTIME_DIR
diagnostics temp directory -> $DIAGNOSTICS_TMP
```

Normalization rules:

- normalize each `PATH` entry independently;
- preserve absolute system paths such as `/usr/bin` and `/usr/lib64`;
- never serialize the raw username or UID merely because it appears in a path;
- do not normalize arbitrary unrelated paths into misleading tokens;
- after normalization, scan generated text for the raw repository root, home
  path, runtime directory, username, and credential-like values;
- privacy scan failure makes doctor or bundle fail;
- bundle manifest must record which token substitutions were applied.

Stable check statuses:

- `pass`;
- `warning`;
- `fail`;
- `unavailable`;
- `skipped`.

### Shared `tools/dev` exit codes

These exit codes are shared by `doctor`, `bundle`, and `run`:

| Code | Meaning |
| --- | --- |
| `0` | success |
| `2` | invalid arguments or unsupported target/mode |
| `3` | explicitly requested optional capability unavailable |
| `4` | required executable or build artifact missing |
| `5` | doctor or renderer-probe diagnostic failure |
| `6` | bundle creation failure or privacy refusal |
| `7` | child process or renderer probe timeout |
| `8` | renderer/example process failed or returned nonzero |
| `9` | rendered-frame marker missing, malformed, contradictory, or duplicated |

Exit-code rules:

- doctor with `overall_status = pass` or `warning` exits `0`;
- doctor with `overall_status = fail` exits `5`;
- ordinary unavailable optional capabilities do not fail doctor;
- explicitly requesting one through `run` or `--validation` exits `3`;
- missing executable exits `4` and prints the exact tracked CMake build command;
- bundle privacy-scan failure exits `6`;
- bundle failure must remove temporary output and leave no valid-looking final
  bundle;
- child timeout exits `7`;
- a renderer process that exits nonzero exits `8`;
- exit code `0` without a valid success marker exits `9`;
- when multiple failures occur, use the first failure in command execution order
  and report all safely known diagnostics to stderr.

## Diagnostic bundle

The implementation must define:

```text
./tools/dev bundle
```

It must create a timestamped, privacy-reviewed bundle under:

```text
build/diagnostics/<timestamp>/
```

`./tools/dev bundle` always generates a fresh `system.json`. It must not reuse
old diagnostic output in 0015.

Include:

- `system.json`;
- human-readable doctor report;
- human-readable manifest;
- exact command lines;
- selected non-secret environment values;
- CMake configure summary;
- engine structured log;
- renderer/backend summary;
- smoke-test result.

Bundle behavior:

- local-only;
- no upload;
- no network submission;
- no home-directory scan;
- allowlist-based file inclusion;
- list every included file in the manifest;
- list every omitted or redacted category in the manifest;
- list every path-normalization token substitution applied in the manifest;
- refuse or warn before including unexpected files;
- use atomic temporary output and rename on success;
- avoid leaving a valid-looking partial bundle after failure.

Do not include tokens, SSH keys, complete environment dumps, home-directory
contents, browser data, JetBrains account files, arbitrary `.idea` state, or
arbitrary dotfiles.

## Standard run commands

The implementation must define one coherent CLI instead of unrelated magic
scripts:

```text
./tools/dev run --mode headless clear_color
./tools/dev run --mode windowed-software clear_color
./tools/dev run --mode windowed-hardware clear_color
./tools/dev run --mode headless --validation clear_color
./tools/dev run --mode windowed-software --validation clear_color
./tools/dev run --mode windowed-hardware --validation clear_color
```

The command must remain discoverable through:

```text
./tools/dev run --help
```

Each run must:

- use already-built executables from tracked preset output locations;
- never configure or build implicitly;
- when an executable is missing, print the exact appropriate command, for
  example `cmake --build --preset linux-gcc-rel`;
- set only required environment variables;
- record the exact command;
- capture logs;
- produce a clear pass/fail result;
- identify selected SDL and Vulkan backends;
- identify renderer backend and selected device where available;
- write results under `build/diagnostics/` or `build/test-output/`;
- never treat process exit code `0` alone as rendered success.

Mode requirements:

- `headless`: required, CI-capable, requires no compositor, and uses headless
  renderer behavior with deterministic software Vulkan settings where
  applicable;
- `windowed-software`: local/manual by default, supported when lavapipe is
  installed and detected, and fails actionably when explicitly invoked while
  unavailable;
- `windowed-hardware`: local/manual, unavailable in ordinary CI, attempts
  default hardware presentation, and reports selected GPU, Vulkan driver,
  backend, swapchain result, and issue #14-class presentation failure details.

Validation is not a fourth mode. `--validation` is an orthogonal flag:

- absence is informational during `doctor`;
- explicit `--validation` invocation fails nonzero when required validation
  support is unavailable;
- failure must include an actionable installation hint;
- validation output is captured in diagnostics where available.

CI must not claim visible-window coverage without an explicitly scoped
compositor harness.

## Explicit rendered-frame marker

The implementation must define a stable machine-readable rendered-frame marker
owned by `examples/clear_color/` or its structured logging surface, not by the
public renderer API.

The marker is exactly one stdout line beginning with `ENGINE_RENDERED_FRAME`
followed by one ASCII space and then the JSON object. The prefix token is:

```text
ENGINE_RENDERED_FRAME
```

The remainder of the line is one compact JSON object. Required fields:

```json
{"schema_version":1,"event":"engine.rendered_frame","mode":"headless","frame_index":5,"result":"readback_verified","backend":"vulkan","device":"llvmpipe"}
```

Marker contract:

- `schema_version` is exactly `1`;
- `event` is exactly `engine.rendered_frame`;
- `mode` is one of `headless`, `windowed-software`, or `windowed-hardware`;
- `frame_index` is a non-negative integer;
- `result` is `readback_verified` for headless mode and `submitted` for
  windowed modes;
- `backend` is a non-empty string;
- `device` is a non-empty string;
- JSON escaping handles spaces and quotes in string values;
- exactly one valid success marker is required per smoke invocation.

Emission rules:

- headless emits only after successful render and verified readback;
- windowed emits only after a real frame submission succeeds;
- skipped, unavailable, cancelled, or failed frames emit no success marker;
- failure and skipped states are reported through stderr, structured
  diagnostics, and exit codes, not through `ENGINE_RENDERED_FRAME`;
- malformed or duplicate success markers cause wrapper exit code `9`;
- process exit code `0` alone is never rendering success.

## Issue #14 diagnostic support

0015 must make hardware Vulkan presentation state precise and reproducible, but
it must not claim to fix proprietary driver or host/container GPU interop.
Native Fedora host RTX 5090 Wayland presentation now succeeds; the old Toolbx
dmabuf/swapchain failure remains useful historical/container diagnostic
evidence.

Required behavior:

- detect `/dev/dri` visibility;
- report Wayland and X11 display variables;
- report Vulkan ICD files and physical devices;
- distinguish lavapipe/software success from hardware success;
- report selected GPU, Vulkan driver, renderer backend, swapchain result, and
  actionable presentation failure details;
- report NVIDIA device/driver visibility without secrets;
- preserve a clean `unavailable` result when hardware GPU access is not present;
- never claim `windowed-hardware` success without the rendered-frame marker.

## Tests and CI

Implementation must define:

- schema validation;
- required fields and enums;
- malformed schema rejection;
- redaction;
- no secret-like environment leakage;
- path normalization;
- bundle allowlist behavior;
- partial bundle cleanup;
- CLI argument parsing;
- unsupported target and mode handling;
- missing executable behavior;
- validation unavailable behavior;
- marker present;
- marker missing;
- marker emitted too early;
- renderer probe timeout;
- command recording and exit-status preservation;
- preservation of the existing fast engine tests and slow renderer suite.

CI may require:

- doctor schema generation;
- bundle/redaction tests;
- headless software Vulkan smoke;
- rendered-frame marker validation;
- wrapper parsing and failure semantics.

CI must not claim:

- real desktop compositor coverage;
- visible windowed software rendering;
- RTX/NVIDIA hardware presentation;
- interactive input coverage.

CI must not require an NVIDIA GPU. Hardware-windowed mode must report
`unavailable` in ordinary CI.

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
- Toolbx adapter implementation;
- IDE-specific launch configuration;
- automatic diagnostic upload;
- Tracy dependency or instrumentation;
- RenderDoc automated capture integration;
- Nsight Graphics;
- Nsight Systems;
- AMD uProf;
- VTune;
- Linux `perf` or heaptrack orchestration;
- GDB/LLDB launch profiles;
- Radeon GPU Profiler;
- GFXReconstruct automation;
- interactive input smoke from issue #19;
- replacement build system around CMake;
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
- GDB/LLDB launch profiles;
- Radeon tools for future AMD GPU machines;
- GFXReconstruct;
- RHI observability requirements such as debug labels, timestamps, breadcrumbs,
  and device-fault reporting.

0015 must not detect, configure, orchestrate, or require RenderDoc, Tracy,
Nsight, VTune, `perf`, heaptrack, GDB, or LLDB. Schema-v1
`optional_capabilities` is limited to capabilities directly required by doctor
and the three run modes: lavapipe, Vulkan validation layers, graphical
session/compositor visibility, hardware Vulkan device visibility, and renderer
probe availability. Profiling, capture, tracing, and debugger integration
require later dedicated specs.

Do not assign new spec numbers for these follow-ups in this PR.

## Acceptance criteria

- [ ] `./tools/dev doctor` writes a concise human report.
- [ ] `./tools/dev doctor` writes `build/diagnostics/system.json`.
- [ ] `./tools/dev doctor --probe-renderer` runs only an existing built headless
      renderer probe with a timeout and reports `unavailable` when absent.
- [ ] `system.json` schema version 1 includes `schema_version`,
      `generated_at`, `overall_status`, `host`, `repository`, `toolchain`,
      `build`, `graphics`, `optional_capabilities`, `checks`, and `redaction`.
- [ ] `system.json` follows the normative schema-v1 field table, complete
      example, stable check IDs, and `overall_status` derivation rules.
- [ ] Check IDs, statuses, severities, summaries, observed values, expected
      values, and remediation fields follow the documented schema.
- [ ] Doctor is useful offline and does not require GitHub authentication.
- [ ] Default doctor does not configure, build, open a window, mutate generated
      state outside its own atomic `build/diagnostics/system.json` output,
      install packages, use `sudo`, scan home, dump the full environment, or run
      substantial GPU work.
- [ ] Doctor never dumps complete environment state or secrets.
- [ ] Doctor and bundle apply deterministic path normalization and fail on
      privacy scan failure.
- [ ] `./tools/dev bundle` creates a fresh timestamped diagnostic bundle.
- [ ] Generated bundle contains no secrets or home-directory dumps.
- [ ] Generated bundle is local-only, allowlist-based, includes a manifest,
      lists included files and redacted/omitted categories, and does not leave a
      valid-looking partial bundle after failure.
- [ ] `./tools/dev run --mode headless clear_color` passes in the supported
      environment.
- [ ] `./tools/dev run --mode windowed-software clear_color` passes with
      lavapipe when explicitly available and fails actionably when unavailable.
- [ ] `./tools/dev run --mode windowed-hardware clear_color` reports precise
      diagnostics and does not claim success on clean initialization failure.
- [ ] `./tools/dev run --mode ... --validation clear_color` captures validation
      output where available and fails actionably when requested support is
      unavailable.
- [ ] `tools/dev run` does not configure or build implicitly and prints the
      exact CMake build command when a required executable is absent.
- [ ] `doctor`, `bundle`, and `run` follow the shared `tools/dev` exit-code
      table.
- [ ] Exact command lines are recorded for doctor, bundle, and run commands.
- [ ] Renderer/backend/device reporting is included in run output.
- [ ] Explicit rendered-frame marker is emitted by `clear_color` or its
      structured logging surface as exactly one stdout
      `ENGINE_RENDERED_FRAME ` JSON line and verified by the wrapper.
- [ ] Process exit code `0` without the rendered-frame marker is treated as
      failure for smoke success.
- [ ] CI validates tooling schema behavior, bundle/redaction behavior, wrapper
      parsing, failure semantics, and headless software Vulkan smoke without an
      NVIDIA GPU.
- [ ] CI does not claim real desktop compositor coverage, visible windowed
      software rendering, RTX/NVIDIA hardware presentation, or interactive input
      coverage.
- [ ] Existing fast engine tests remain green.
- [ ] Existing slow renderer validation remains green where supported.

## Implementation sequencing

0015 remains one bounded implementation PR after 0014 is implemented. The
implementation order is:

1. define `system.json` schema version 1 and redaction policy;
2. implement read-only `doctor`;
3. implement `doctor --probe-renderer`;
4. implement fresh local diagnostic bundles;
5. add the `clear_color` rendered-frame marker;
6. implement explicit `run` modes as thin wrappers over already-built tracked
   preset outputs;
7. add schema, redaction, CLI, marker, probe-timeout, and smoke tests;
8. add CI coverage only for headless/software and wrapper semantics.

If optional debugger/profiler/capture integrations grow beyond detection and
availability reporting, split that work into future specs. Keep 0015 focused on
doctor, bundle, explicit run modes, and rendered-frame verification.
