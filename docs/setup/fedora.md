# Fedora setup

**This is the primary (only) supported platform through M4.** See `docs/adr/0002-fedora-primary-platform.md`.

Target: Fedora 40+ host with a Fedora Rawhide toolbox for the GCC 16 / Clang 20
toolchain until those compiler versions ship in a stable Fedora release.

## Quick path — toolbox with GCC 16

If your host Fedora has an older compiler and you want GCC 16 without disturbing
the host:

```sh
# One-time
sudo dnf install -y toolbox
toolbox create --image registry.fedoraproject.org/fedora:rawhide engine
toolbox enter engine
# inside the toolbox, install GCC 16 + everything else (see "Required packages" below)
```

The toolbox shares your home directory, so the repo at `~/dev/projects/engine` is visible inside. Build artifacts in `build/` will be toolbox-specific — that's fine.

## Required packages

```sh
sudo dnf install -y \
    gcc gcc-c++ clang clang-tools-extra lld mold \
    cmake ninja-build sccache \
    git pre-commit \
    python3 python3-pip \
    autoconf autoconf-archive automake libtool perl-core \
    libX11-devel libXcursor-devel libXrandr-devel libXi-devel libXtst-devel \
    libXinerama-devel mesa-libGL-devel mesa-libEGL-devel \
    mesa-vulkan-drivers vulkan-loader-devel vulkan-tools \
    alsa-lib-devel pulseaudio-libs-devel \
    pkgconf-pkg-config tar curl
```

**Verify compiler versions** — the engine requires **GCC 16+** and **Clang 20+** (ADR 0002):

```sh
g++ --version       # must report 16.x
clang++ --version   # must report 20.x
cmake --version     # must be 3.30+
ninja --version     # must be 1.12+
```

If GCC 16 isn't available in your Fedora release, use the toolbox path above or build GCC 16 from source under `~/opt/gcc16` and set `CC` / `CXX` in your shell rc.

## vcpkg

```sh
git clone --depth 1 https://github.com/microsoft/vcpkg ~/vcpkg
~/vcpkg/bootstrap-vcpkg.sh -disableMetrics
echo 'export VCPKG_ROOT=$HOME/vcpkg' >> ~/.bashrc
echo 'export CMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake' >> ~/.bashrc
source ~/.bashrc
```

## First build

```sh
cd ~/dev/projects/engine
pre-commit install                       # clang-format/tidy/codespell/commitlint hooks
cmake --preset linux-clang-asan
cmake --build --preset linux-clang-asan
ctest --preset linux-clang-asan          # only [fast] tests
./build/linux-clang-asan/Debug/examples/hello_window/hello_window
```

Expected: tests pass, `hello_window` prints `frame 0` through `frame 4`.

The fast inner loop:

```sh
cmake --workflow --preset check          # configure + build + [fast] tests, ~10s incremental
```

## Editor / clangd setup

The `linux-clang-asan` preset exports a compile database for clangd. After the first successful workflow run, link it at the repo root and restart your editor or clangd:

```sh
cmake --workflow --preset check
ln -sfn build/linux-clang-asan/compile_commands.json compile_commands.json
```

## Headless rendering (CI parity)

Screenshot tests pin to the **llvmpipe** software rasterizer for bit-stable output across machines:

```sh
sudo dnf install -y mesa-vulkan-drivers vulkan-tools
export VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json
vulkaninfo --summary | head -20          # should show llvmpipe device
```

`ENGINE_HEADLESS=1` together with these env vars is what CI uses. Don't run screenshot tests against your real GPU — goldens won't match.

## Troubleshooting

- **`cmake: command not found`:** Fedora 40 ships 3.28. Use the toolbox image (Fedora 41 has 3.30) or `pip install --user 'cmake>=3.30'` and add `~/.local/bin` to `PATH`.
- **GCC < 16:** see toolbox path above. Do not try to make a GCC 15 build pass — ADR 0002 raises the floor for a reason.
- **vcpkg first-build is slow:** confirm `sccache` is on `PATH` and `CMAKE_CXX_COMPILER_LAUNCHER=sccache` is set by the preset. Cold first configure (SDL3 + SDL3 GPU + spdlog + glm + miniaudio) ~3–6 min; subsequent rebuilds ~30s cached.
- **`mold: undefined reference`:** confirm `CMAKE_LINKER_TYPE=MOLD` is the preset's value (it is). If your distro's `mold` is older than 2.30, install from Mold's release page.
- **Permission denied writing to `~/vcpkg`:** clone to your home dir, not `/opt` or `/usr/local`.
- **GitHub Actions container fails on dnf install:** check the CI file — it pins `fedora:rawhide`. Update both the workflow and this doc together if you bump.
