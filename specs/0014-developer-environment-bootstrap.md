# 0014 — Developer environment and IDE bootstrap

- Owner: TBD
- Milestone: M1
- Status: in-review
- Tracking issue: https://github.com/arturasan/game-engne-new/issues/16
- Implementation PR: https://github.com/arturasan/game-engne-new/pull/18
- Merged in: TBD

## Scope and motivation

Spec 0014 is the M1 developer-environment bootstrap cleanup after renderer spec
0005 and before diagnostics spec 0015 and ordinary feature work resume. It is
not an engine runtime feature and does not add runtime dependencies.

The original draft assumed Fedora Kinoite plus Toolbx as the daily development
environment. The development machine has since migrated to regular Fedora KDE,
so the PR is amended to make host-native Fedora development the primary path.
Toolbx remains only an optional future clean-room or CI-parity adapter.

The bootstrap must solve repository-local setup problems without embedding
machine-local IDE state:

- no daily dependence on inherited `VCPKG_ROOT`;
- no shell-profile `CMAKE_TOOLCHAIN_FILE` requirement;
- repo-local vcpkg under `.cache/dev/vcpkg`;
- CMake targets and tracked CMake presets as the authoritative
  configure/build/test contract;
- root `compile_commands.json` for clangd editors;
- normal host CLion startup without a generated wrapper or desktop launcher.

Bevy is not an authority for this tooling task.

## Official references

Implementation work must use current official primary documentation when
changing contracts:

- CMake Presets manual:
  https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html
- CMake toolchains manual:
  https://cmake.org/cmake/help/latest/manual/cmake-toolchains.7.html
- vcpkg CMake integration:
  https://learn.microsoft.com/vcpkg/users/buildsystems/cmake-integration
- vcpkg manifest mode:
  https://learn.microsoft.com/vcpkg/concepts/manifest-mode
- JetBrains CLion CMake Presets:
  https://www.jetbrains.com/help/clion/cmake-presets.html
- JetBrains CLion toolchains:
  https://www.jetbrains.com/help/clion/how-to-create-toolchain-in-clion.html
- Fedora Toolbx overview:
  https://docs.fedoraproject.org/en-US/fedora-silverblue/toolbox/

## Supported development model

Primary M1 path:

1. Clone the repository anywhere under the developer's home directory.
2. Install the Fedora host prerequisites from `docs/setup/fedora.md`.
3. Run `./tools/dev bootstrap` once to create or repair repo-local generated
   state.
4. Configure, build, test, run, and debug using the tracked CMake presets.
5. Open CLion normally on the host if desired; it consumes the same tracked
   presets.

Daily work must not require:

- creating or entering a Toolbx container;
- launching CLion through a generated wrapper;
- generated desktop entries;
- generated `CMakeUserPresets.json`;
- committed `.idea` state;
- personal absolute paths in tracked files.

Optional parity path:

- no Toolbx files, containers, or launch wrappers are part of the 0014
  implementation.
- future Toolbx support, if needed, must be an explicit adapter rather than the
  daily build contract.
- Windows and macOS remain future adapters and are not implemented by 0014.

## Bootstrap contract

The implementation defines:

```text
./tools/dev bootstrap
./tools/dev bootstrap --check
```

Required behavior:

- idempotent and safe to rerun;
- `--check` is read-only and reports what bootstrap would create or repair;
- verifies host tools: GCC, G++, Clang, Clang++, clangd, CMake, Ninja, mold,
  sccache, Git, Python, pre-commit, Vulkan tools, `pkg-config`, curl, tar, zip,
  and unzip;
- verifies host development-library `pkg-config` modules needed by current
  vcpkg dependencies, including Vulkan loader development metadata;
- bootstraps vcpkg under `.cache/dev/vcpkg` with metrics disabled;
- checks out vcpkg at the repository `builtin-baseline`, not a moving branch;
- installs the repository pre-commit hook;
- creates `compile_commands.json` as a symlink to
  `build/linux-clang-asan/compile_commands.json`;
- reports actionable failures with expected path/version, observed
  path/version, and suggested recovery where practical;
- does not run `sudo`, install packages, install GPU drivers, modify shell
  profiles, create Toolbx containers, generate CLion wrappers, generate desktop
  launchers, or write `.idea`.

Bootstrap success must not depend on `export VCPKG_ROOT=...` in `.bashrc`,
`.profile`, or terminal-only state.

## CMake and vcpkg contract

Repo-local vcpkg root is:

```text
.cache/dev/vcpkg
```

The tracked hidden Linux base preset uses:

```json
"toolchainFile": "${sourceDir}/.cache/dev/vcpkg/scripts/buildsystems/vcpkg.cmake"
```

After bootstrap, these tracked standard names must work with `VCPKG_ROOT`
unset:

```text
cmake --preset linux-clang-asan
cmake --preset linux-gcc-rel
cmake --workflow --preset check
```

Tracked preset responsibilities:

- stable preset names;
- build/test/workflow intent;
- portable generator choice;
- build directory shape;
- non-secret default cache variables;
- repo-local vcpkg toolchain path;
- compiler names, not machine-specific absolute compiler paths.

Generated local state responsibilities:

- `.cache/dev/vcpkg`;
- `compile_commands.json`;
- `.git/hooks/pre-commit`;

Ignored generated state must remain safe to delete and regenerate.

Tool selection:

- `sccache` remains the compiler launcher;
- `mold` remains the linker selection;
- Ninja Multi-Config remains the Linux generator;
- Debug maps to `linux-clang-asan` / `Debug`;
- Release maps to `linux-gcc-rel` / `Release`;
- bootstrap detects stale `/usr/lib64/ccache/*` compiler paths recorded in
  CMake caches and prints the exact cleanup command.

## CLion acceptance contract

CLion is a host-native optional editor path. Acceptance for the installed CLion
version:

- CLion starts normally from the host desktop entry or JetBrains Toolbox;
- the project opens at the repository path selected by the user;
- no generated wrapper or generated desktop launcher is required;
- no manual `VCPKG_ROOT`, `CMAKE_TOOLCHAIN_FILE`, compiler, Ninja, Debug, or
  Release injection is required after bootstrap;
- the tracked CMake presets configure successfully;
- CLion code model, clangd/indexing, compiler information, build, CTest
  discovery, run, and debug work;
- no committed user-specific `.idea` state is added.

Manual CLion run/debug and graphical smoke validation require the installed IDE
and desktop session. They were validated on the regular Fedora KDE development
machine before accepting 0014.

## Current PR evidence

Draft PR #18 now supports the host-native Fedora contract:

- `./tools/dev bootstrap`;
- `./tools/dev bootstrap --check`;
- repo-local vcpkg at `.cache/dev/vcpkg`;
- root `compile_commands.json` symlink for clangd;
- pre-commit hook installation;
- tracked presets as the authoritative build/test/run contract.

The implementation intentionally removed the Toolbx-first daily path and the
generated CLion wrapper/desktop/run-configuration templates. Normal host CLion
startup is the supported IDE path.

Manual validation recorded for draft PR #18 on the regular Fedora KDE
development machine confirmed:

- CLion launched normally from KDE and opened
  the canonical repository checkout path;
- tracked CMake presets configured without manually setting `VCPKG_ROOT`;
- indexing, compiler information, build, CTest discovery, individual test run,
  and breakpoint debugging worked;
- native `linux-gcc-rel` Release `clear_color` displayed the blue Wayland
  window on the RTX 5090;
- Git status remained clean after validation;
- `./tools/dev bootstrap --check` passed after the manual IDE and graphics
  validation.

## Tests and CI

Implementation must define:

- focused tooling tests for bootstrap parsing and generated-state behavior;
- tests proving `--check` is read-only;
- tests proving bootstrap uses `.cache/dev/vcpkg`;
- tests proving bootstrap does not generate CLion wrappers, desktop launchers,
  `.idea`, or `CMakeUserPresets.json`;
- CI smoke for bootstrap validation that does not unexpectedly modify the CI
  host;
- manual CLion acceptance on the primary development machine.

CI must not require a graphical desktop or NVIDIA GPU for 0014.

## Files allowed

The 0014 implementation PR is expected to stay within this bounded surface:

- `tools/dev`;
- `tools/dev.d/**`;
- `.gitignore`;
- `docs/setup/fedora.md`;
- narrow tooling tests;
- narrow CI changes only for bootstrap checks;
- `specs/0014-developer-environment-bootstrap.md`;
- roadmap/development-plan/example-map metadata.

This does not grant permission to redesign engine runtime systems.

## Out of scope

0014 explicitly excludes:

- `./tools/dev doctor`;
- full human diagnostics report;
- versioned `build/diagnostics/system.json`;
- diagnostic bundles;
- run-mode framework;
- rendered-frame success marker;
- renderer/backend/device reporting;
- issue #14 hardware-presentation diagnosis beyond bootstrap feasibility;
- Tracy, RenderDoc, Nsight, AMD tools, VTune, Radeon GPU Profiler,
  GFXReconstruct, and RHI observability work;
- renderer architecture changes;
- Windows/macOS developer environments;
- automatic installation or modification of proprietary GPU drivers;
- broad CI redesign.

## Acceptance criteria

- [x] `./tools/dev bootstrap` exists and is idempotent.
- [x] `./tools/dev bootstrap --check` is read-only.
- [x] Bootstrap is host-native and does not require Toolbx.
- [x] Toolbx is not implemented as part of the daily bootstrap path.
- [x] Repo-local vcpkg exists at `.cache/dev/vcpkg`.
- [x] vcpkg is checked out at the repository baseline.
- [x] Standard tracked CLI presets work with `VCPKG_ROOT` unset:
      `cmake --preset linux-clang-asan`, `cmake --preset linux-gcc-rel`, and
      `cmake --workflow --preset check`.
- [x] Bootstrap creates a root `compile_commands.json` symlink for clangd.
- [x] Bootstrap does not generate `CMakeUserPresets.json`, `.idea`, CLion
      wrappers, or desktop launchers.
- [x] Generated compiler paths avoid stale `/usr/lib64/ccache/*` compiler
      wrappers.
- [x] No manual environment injection is required for CMake, vcpkg, compiler,
      Ninja, run, or debug setup.
- [x] CMake configure/build/test succeeds from the CLI fallback.
- [x] CLion opens normally on the host without a wrapper.
- [x] CLion code model, compiler information, CTest discovery, run, and debug
      are manually validated on the host.
- [x] Graphical renderer smoke is manually validated on the host.
- [x] Existing fast engine/tooling tests remain green.
