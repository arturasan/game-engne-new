# 0014 — Developer environment and IDE bootstrap

- Owner: TBD
- Milestone: M1
- Status: draft
- Tracking issue: https://github.com/arturasan/game-engne-new/issues/16
- Implementation PR: TBD
- Merged in: TBD

## Scope and motivation

Spec 0014 is the M1 developer-environment bootstrap foundation after renderer
spec 0005 and before diagnostics spec 0015 and ordinary feature work resume. It
is not an engine runtime feature and does not add runtime dependencies.

Renderer work proved that the configured Fedora Toolbx CLI can build and
validate the repository, but also exposed that the daily IDE setup depends on too
much machine-local knowledge:

- desktop-launched CLion did not inherit `VCPKG_ROOT`;
- CLion could not find Ninja in the same way the CLI did;
- CLion selected stale `/usr/lib64/ccache/g++` or
  `/usr/lib64/ccache/clang++` paths instead of the intended compiler binaries;
- the same checkout appeared through both `/home/...` and `/var/home/...`;
- Debug and Release run configurations were confused;
- graphical environment propagation was inconsistent between terminal and
  desktop launch paths;
- moving to a new machine currently requires too much manual setup knowledge.

Separate the causes precisely:

- repository bootstrap problems: inherited `VCPKG_ROOT`, generated-local preset
  gaps, and no canonical local setup contract;
- IDE integration problems: desktop CLion launch did not receive the same
  toolchain, CMake, Ninja, vcpkg, run, and debug context as the working CLI;
- host/container interop: graphical sockets, `/dev/dri`, and canonical paths
  must be verified before assuming Toolbx can be the daily IDE environment;
- hardware-driver limitations: NVIDIA presentation remains tracked by
  https://github.com/arturasan/game-engne-new/issues/14 and is not solved by
  bootstrap.

Do not blame CLion, Toolbx, CMake, Fedora, or the engine without a specific
diagnostic result.

## Official references

Implementation work must use current official primary documentation:

- JetBrains CLion CMake Presets:
  https://www.jetbrains.com/help/clion/cmake-presets.html
- JetBrains CLion CMake profiles:
  https://www.jetbrains.com/help/clion/cmake-profile.html
- JetBrains CLion toolchains:
  https://www.jetbrains.com/help/clion/how-to-create-toolchain-in-clion.html
- JetBrains CLion run/debug configurations:
  https://www.jetbrains.com/help/clion/run-debug-configuration.html
- JetBrains CLion Docker toolchain:
  https://www.jetbrains.com/help/clion/clion-toolchains-in-docker.html
- JetBrains CLion remote development:
  https://www.jetbrains.com/help/clion/remote-development.html
- JetBrains CLion Dev Containers:
  https://www.jetbrains.com/help/clion/connect-to-devcontainer.html
- CMake Presets manual:
  https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html
- CMake toolchains manual:
  https://cmake.org/cmake/help/latest/manual/cmake-toolchains.7.html
- vcpkg CMake integration:
  https://learn.microsoft.com/vcpkg/users/buildsystems/cmake-integration
- vcpkg manifest mode:
  https://learn.microsoft.com/vcpkg/concepts/manifest-mode
- Fedora Kinoite getting started:
  https://docs.fedoraproject.org/en-US/fedora-kinoite/getting-started/
- Fedora Silverblue technical information for `/home -> /var/home`:
  https://docs.fedoraproject.org/en-US/fedora-silverblue/technical-information/
- Fedora Toolbx overview:
  https://docs.fedoraproject.org/en-US/fedora-silverblue/toolbox/
- Toolbx container integration details:
  https://containertoolbx.org/doc/
- Freedesktop Desktop Entry Specification:
  https://specifications.freedesktop.org/desktop-entry-spec/latest-single/

Bevy is not an authority for this tooling task.

## Supported development model

Primary M1 path:

1. Clone the repository anywhere under the developer's home directory.
2. Run `./tools/dev bootstrap` from the checkout.
3. Launch `Game Engine CLion` from the desktop application launcher.
4. CLion opens the project once under the canonical repository root.
5. Configure, build, test, run, and debug using the supported Toolbx build
   environment.

Fallback CLI path:

1. Run `./tools/dev bootstrap --check`.
2. Run repository commands through `./tools/dev` from the same checkout.
3. The wrapper enters the supported Toolbx environment when needed and prints
   the selected environment and exact command.

Selected primary model:

- Toolbx name: `game-engine-dev`
- Generated desktop entry:
  `~/.local/share/applications/game-engine-clion.desktop`
- Generated desktop wrapper: `.cache/dev/bin/open-clion`
- Repo-local vcpkg: `.cache/dev/vcpkg`
- Generated presets: `CMakeUserPresets.json`
- Generated bootstrap output: `build/diagnostics/`
- Canonical root: result of `realpath` on the checkout, normally
  `/var/home/<user>/...` on Fedora Atomic desktops

The daily path must not require launching CLion from a terminal or running a
custom `env ... clion` command.

## Environment pinning contract

The implementation must define a reproducible and reviewable Toolbx
environment. It must add a tracked Toolbx-compatible image definition, exact
immutable image reference, or Containerfile selected during implementation.

Required tracked files in the implementation PR:

- `tools/dev/container/Containerfile` for the Toolbx-compatible development
  image;
- `tools/dev/container/fedora-packages.txt` for the package manifest used by
  bootstrap.

The Containerfile must use an exact Fedora base image tag plus digest where the
registry supports digests. The final contract must not use an unbounded
`rawhide:latest` image. Package versions do not all need hard RPM version pins
if that makes Fedora maintenance impractical, but the image identity and package
manifest must be reproducible and reviewable.

Updates to the base image, digest, or package manifest happen through normal
pull requests. Bootstrap output must record the actual image ID and installed
tool versions.

## Bootstrap contract

The implementation must define:

```text
./tools/dev bootstrap
./tools/dev bootstrap --check
```

Required behavior:

- idempotent and safe to rerun;
- `--check` is read-only and reports what bootstrap would create or repair;
- does not silently delete Toolbx environments, vcpkg installs, build
  directories, CLion settings, desktop files, or user files;
- reports every privileged operation before running it;
- refuses privileged host package operations unless explicitly requested;
- creates or verifies Toolbx environment `game-engine-dev`;
- installs or verifies GCC, G++, Clang, Clang++, CMake, Ninja, mold, sccache,
  Git, Python, pre-commit, Vulkan loader/tools, Wayland/X11 development
  packages, and current vcpkg build dependencies inside the development
  environment;
- bootstraps vcpkg under `.cache/dev/vcpkg` with metrics disabled;
- checks out the vcpkg clone at the repository's configured vcpkg baseline or a
  tracked known revision derived from that baseline, not a moving branch;
- installs pre-commit hooks;
- generates `CMakeUserPresets.json`;
- generates `.cache/dev/bin/open-clion`;
- generates `~/.local/share/applications/game-engine-clion.desktop`;
- writes a minimal bootstrap report under `build/diagnostics/`;
- produces actionable failures with the failing command, expected path/version,
  observed path/version, and suggested recovery.

Bootstrap must not require a permanent shell-profile modification. Success must
not depend on `export VCPKG_ROOT=...` in `.bashrc`, `.profile`, or terminal-only
state.

## CMake and vcpkg contract

The implementation must remove daily dependence on inherited `VCPKG_ROOT`.

Repo-local vcpkg root is `.cache/dev/vcpkg`.

### Standard tracked presets

The 0014 implementation PR must change the tracked hidden Linux base preset to
use the repo-local toolchain directly:

```json
"toolchainFile": "${sourceDir}/.cache/dev/vcpkg/scripts/buildsystems/vcpkg.cmake"
```

If the active CMake schema requires a different spelling, use the equivalent
supported relative path and document the reason in the implementation PR.

After bootstrap, these tracked standard names must work without `VCPKG_ROOT`:

```text
cmake --preset linux-clang-asan
cmake --preset linux-gcc-rel
cmake --workflow --preset check
```

The Toolbx environment provides stable resolution of:

- `clang++`;
- `g++`;
- `ninja`;
- `sccache`;
- `mold`.

The standard presets must not require machine-specific compiler absolute paths.

### Generated user presets

`CMakeUserPresets.json` is optional generated IDE-local configuration.

It may:

- define CLion-specific aliases;
- set `cmakeExecutable`;
- add CLion vendor fields;
- set local environment needed only by the IDE.

It must not:

- duplicate any existing tracked preset name;
- be required for the standard CLI preset names to work;
- restore reliance on inherited `VCPKG_ROOT`.

If aliases are needed, require distinct names such as:

```text
clion-linux-clang-asan
clion-linux-gcc-rel
```

They should inherit the tracked standard configure presets and override only
genuinely IDE-local fields.

Tracked versus generated:

- tracked: stable preset names, build/test/workflow intent, portable generator
  choice, build directory shape, non-secret default cache variables, repo-local
  vcpkg toolchain path, and templates needed to generate local IDE aliases;
- generated: machine-specific CMake executable path, CLion vendor fields, local
  IDE-only environment, canonical repo path, and CLion launcher references;
- ignored: `.cache/dev/`, `CMakeUserPresets.json`, diagnostic outputs, and
  generated local CLion state.

Tool selection:

- compilers must resolve to intended binaries, not stale `/usr/lib64/ccache/*`
  wrapper paths;
- `sccache` remains the compiler launcher;
- `mold` remains the linker selection;
- Ninja Multi-Config remains the Linux generator;
- Debug maps to `linux-clang-asan` / `Debug`;
- Release maps to `linux-gcc-rel` / `Release`;
- bootstrap must detect stale compiler paths recorded in CMake cache or CLion
  local state and print the exact cleanup command.

## Clean-build contract

The implementation must define:

```text
./tools/dev clean-build <preset>
```

Required behavior:

- accepts only known CMake preset names;
- prints the selected binary directory before deleting it;
- removes only the selected CMake binary directory;
- never deletes `.cache/dev/vcpkg`, Toolbx environments, user home files, or
  unrelated build directories;
- supports `--dry-run` or a confirmation-free check mode.

## Canonical path handling

Bootstrap resolves the checkout using `realpath`. On Fedora Atomic desktops the
normalized canonical root is normally `/var/home/...` because `/home` is a
symlink to `/var/home`.

Required behavior:

- generated presets, launchers, run configurations, and bootstrap reports use
  the single canonical path;
- tracked files contain no usernames or machine-specific absolute paths;
- opening the same checkout through `/home/...` and `/var/home/...`
  simultaneously is rejected or warned clearly;
- bootstrap prints the canonical path and the noncanonical path when they differ;
- CLion acceptance must show only one project root for the checkout.

Do not require the user to clone by typing `/var/home/...`; canonicalization is
the bootstrap's responsibility.

## CLion feasibility gate

Before implementing the full bootstrap system, the 0014 implementation PR must
perform a bounded feasibility spike with a clean CLion user/profile state or an
equivalent clean user state. It must prove:

1. the generated desktop entry starts CLion without a terminal;
2. CLion actually uses the Toolbx environment for CMake/compiler/Ninja;
3. the launcher does not hand the project to an already-running incompatible
   host CLion process;
4. only one canonical project root is opened;
5. configure succeeds;
6. indexing and compiler information load;
7. build and CTest discovery work;
8. run and debug work;
9. `hello_window` receives keyboard/window events;
10. `clear_color` runs with lavapipe.

If this feasibility spike fails, the implementation must stop before building
the full bootstrap system and report whether the primary model needs a small ADR
or spec amendment.

## CLion process-instance policy

The generated launcher must not silently reuse an incompatible already-running
CLion instance.

It must either:

- use an officially supported isolated backend/configuration mechanism; or
- detect a conflicting CLion process and show an actionable message asking the
  developer to close it.

Rules:

- do not rely on undocumented JVM flags;
- do not modify JetBrains installation files;
- discover the JetBrains Toolbox CLion installation through a stable documented
  mechanism or generated local configuration;
- tracked files must not contain a versioned CLion installation path;
- the implementation PR must record the exact mechanism chosen.

## CLion acceptance contract

Acceptance for the installed CLion version:

- after bootstrap, CLion starts from the normal desktop application launcher
  entry `Game Engine CLion`;
- no terminal-launched `env ... clion` command is an acceptable daily solution;
- project opens once under the canonical root;
- no manual `VCPKG_ROOT`, `CMAKE_TOOLCHAIN_FILE`, compiler, Ninja, CMake option,
  Debug/Release, or run-environment injection is required;
- CMake configure succeeds for the intended profile;
- CLion code model, clangd/indexing, and compiler information are healthy;
- build works from the IDE;
- CTest discovery works;
- run and debug work;
- `hello_window` receives keyboard/window input;
- `clear_color` runs in supported lavapipe software-windowed mode;
- no committed user-specific `.idea` state is added unless a later review proves
  a portable subset is genuinely supportable.

Generated local CLion state is allowed only if it is ignored, documented,
regenerable, and safe to delete.

## Minimal bootstrap verification

0014 may write a minimal bootstrap report under:

```text
build/diagnostics/
```

It may include:

- schema version and timestamp;
- canonical repo path;
- Toolbx name and image ID;
- generated file paths;
- selected CMake, Ninja, compiler, mold, sccache, and vcpkg paths;
- bootstrap/check pass/fail status.

The full doctor report, diagnostic bundle, renderer backend/device report, run
modes, and rendered-frame success marker belong to spec 0015.

## Tests and CI

Implementation must define:

- fast tests for bootstrap configuration parsing and generated-file rendering;
- tests proving `--check` is read-only;
- tests proving generated presets use `.cache/dev/vcpkg`;
- negative tests for missing Toolbx, missing CLion, missing Ninja, stale
  compiler paths, and noncanonical paths;
- CI smoke for bootstrap validation that does not unexpectedly modify the CI
  host;
- manual clean-room CLion acceptance on the primary development machine.

CI must not require a graphical desktop or NVIDIA GPU for 0014.

## Files allowed

The 0014 implementation PR is expected to stay within this bounded surface:

- `tools/dev/**`;
- `tools/dev/container/Containerfile`;
- `tools/dev/container/fedora-packages.txt`;
- generated-preset template files;
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

- [ ] `./tools/dev bootstrap` exists and is idempotent.
- [ ] `./tools/dev bootstrap --check` is read-only.
- [ ] Toolbx environment `game-engine-dev` is created or verified.
- [ ] Toolbx image identity and installed tool versions are recorded.
- [ ] The development image definition and package manifest are tracked and
      reviewable.
- [ ] Repo-local vcpkg exists at `.cache/dev/vcpkg`.
- [ ] vcpkg is checked out at the repository baseline or tracked known revision.
- [ ] Standard tracked CLI presets work with `VCPKG_ROOT` unset:
      `cmake --preset linux-clang-asan`, `cmake --preset linux-gcc-rel`, and
      `cmake --workflow --preset check`.
- [ ] `CMakeUserPresets.json` generated preset names do not collide with tracked
      preset names.
- [ ] Deleting and regenerating `CMakeUserPresets.json` does not break the
      standard CLI workflow.
- [ ] CLion can use either the standard presets directly or distinct generated
      aliases such as `clion-linux-clang-asan` and `clion-linux-gcc-rel`.
- [ ] Generated compiler paths avoid stale `/usr/lib64/ccache/*` compiler
      wrappers.
- [ ] `.cache/dev/bin/open-clion` is generated.
- [ ] `~/.local/share/applications/game-engine-clion.desktop` is generated.
- [ ] CLion process-instance policy is implemented and documented.
- [ ] Canonical path is resolved with `realpath` and used in generated local
      files.
- [ ] Duplicate `/home` and `/var/home` roots are rejected or clearly warned.
- [ ] Desktop CLion opens normally after bootstrap.
- [ ] No manual environment injection is required for CMake, vcpkg, compiler,
      Ninja, run, or debug setup.
- [ ] CMake configure/build/test succeeds from the CLI fallback.
- [ ] CMake configure/build/test succeeds from CLion.
- [ ] CLion code model, compiler information, and CTest discovery work.
- [ ] IDE run and debug work.
- [ ] `hello_window` receives input from the IDE-launched run.
- [ ] `clear_color` runs with lavapipe from the IDE-launched run.
- [ ] `./tools/dev clean-build <preset>` removes only the selected build
      directory.
- [ ] Existing 52 fast engine tests remain green.
- [ ] Clean-room CLion acceptance is recorded before marking 0014 implemented.

## Implementation sequencing

0014 is one bounded implementation PR. It must complete the CLion feasibility
gate before implementing the full bootstrap system. If the feasibility gate
fails, stop and report the required ADR or spec amendment instead of building a
large partial bootstrap.
