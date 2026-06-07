# Fedora setup

Fedora is the only supported development platform through M4. See
`docs/adr/0002-fedora-primary-platform.md`.

The supported M1 workflow is regular Fedora host-native development through the
tracked CMake presets:

```sh
cmake --preset linux-clang-asan
cmake --build --preset linux-clang-asan
ctest --preset linux-clang-asan
cmake --workflow --preset check
```

`./tools/dev` is an optional thin convenience layer. Its bootstrap command
verifies host tools, bootstraps repo-local vcpkg at `.cache/dev/vcpkg`,
installs the repository pre-commit hook, and creates the root
`compile_commands.json` symlink used by clangd editors.

## Required Host Development Packages

The host must provide Fedora with these development tools and libraries for the
normal configure/build/test loop. This list matches what
`./tools/dev bootstrap --check` validates:

```sh
sudo dnf install -y \
  gcc gcc-c++ clang clang-tools-extra mold \
  cmake ninja-build sccache \
  git python3 pre-commit \
  autoconf autoconf-archive automake libtool perl perl-open \
  libX11-devel libXcursor-devel libXrandr-devel libXi-devel libXtst-devel \
  libXinerama-devel mesa-libGL-devel mesa-libEGL-devel \
  wayland-devel wayland-protocols-devel libxkbcommon-devel \
  vulkan-loader-devel vulkan-tools \
  alsa-lib-devel pulseaudio-libs-devel \
  pkgconf-pkg-config curl tar zip unzip
```

Minimum versions checked by `./tools/dev bootstrap`:

- CMake 3.30, from `cmake_minimum_required(VERSION 3.30)`;
- Ninja 1.12, from the documented Fedora build baseline;
- GCC/G++ 16, from ADR 0002's compiler floor;
- Clang/Clang++/clangd 20, from ADR 0002's compiler floor.

Bootstrap detects missing host tools and development-library `pkg-config`
modules, including SDL's X11/Wayland, OpenGL/EGL, audio, and Vulkan development
modules. It does not run `sudo`, install packages, install GPU drivers, modify
shell profiles, create Toolbx containers, or change CLion settings.

## Optional Local ASan/Slow Renderer Parity Packages

These packages are useful when reproducing CI's sanitizer and software-renderer
validation locally, but the default bootstrap does not require them:

```sh
sudo dnf install -y \
  llvm \
  mesa-vulkan-drivers
```

- `llvm` provides `llvm-symbolizer` for clearer ASan/UBSan reports.
- `mesa-vulkan-drivers` provides lavapipe for software Vulkan rendering.

## Bootstrap

From the repository checkout:

```sh
./tools/dev bootstrap --check   # read-only validation
./tools/dev bootstrap           # create or repair repo-local generated state
```

Generated local files are ignored and regenerable:

- `.cache/dev/vcpkg`;
- `compile_commands.json`;
- `.git/hooks/pre-commit`.

Do not add `VCPKG_ROOT` or `CMAKE_TOOLCHAIN_FILE` to shell startup files. The
tracked CMake presets use:

```text
${sourceDir}/.cache/dev/vcpkg/scripts/buildsystems/vcpkg.cmake
```

## Build and test

The standard presets work without `VCPKG_ROOT`:

```sh
env -u VCPKG_ROOT cmake --preset linux-clang-asan
env -u VCPKG_ROOT cmake --preset linux-gcc-rel
env -u VCPKG_ROOT cmake --workflow --preset check
```

The fast inner loop remains:

```sh
cmake --workflow --preset check
```

## Editor setup

clangd-based editors consume the generated root symlink:

```text
compile_commands.json -> build/linux-clang-asan/compile_commands.json
```

Run `./tools/dev bootstrap`, then configure `linux-clang-asan` once before
expecting the symlink target to exist. Zed, VS Code, and other clangd editors
can use the repository root normally after that.

CLion is host-native and optional. Open the project normally from the installed
CLion desktop entry or JetBrains Toolbox. Use the tracked CMake presets inside
CLion; no generated wrapper, generated desktop launcher, shell-profile
environment injection, or committed `.idea` state is part of the daily path.

## Optional Toolbx parity

Toolbx may still be useful for clean-room or CI-parity experiments, but it is
not part of the daily development contract and `./tools/dev bootstrap` does not
create or require a Toolbx container.

## Rendering smoke path

For software Vulkan rendering parity with CI, use lavapipe:

```sh
export VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json
export SDL_GPU_DRIVER=vulkan
export LIBGL_ALWAYS_SOFTWARE=1
export MESA_LOADER_DRIVER_OVERRIDE=llvmpipe
```

`hello_window` is platform-only in M1 and is not expected to present a visible
Wayland surface until a renderer commits a buffer.

## Troubleshooting

- Missing host package: install the reported Fedora package, then rerun
  `./tools/dev bootstrap --check`.
- Broken vcpkg checkout: remove `.cache/dev/vcpkg` only after confirming no
  local work exists there, then rerun `./tools/dev bootstrap`.
- Stale CMake cache: remove the affected generated build directory, such as
  `build/linux-clang-asan`, and configure again.
- Broken clangd includes: confirm `compile_commands.json` points to
  `build/linux-clang-asan/compile_commands.json`, then configure
  `linux-clang-asan`.
