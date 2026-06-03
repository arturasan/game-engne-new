# Windows setup

> **Status: unmaintained until M5.** See `docs/adr/0002-fedora-primary-platform.md`.
>
> Through milestones M0–M4 the engine targets Fedora only. The presets (`win-msvc-dbg`, `win-mingw-rel`, `win-mingw-dbg`), the bootstrap script (`tools/bootstrap-windows-portable.ps1`), and the rest of this document are kept in-tree as reference for the M5 multi-platform expansion, but **no PR is required to keep them green** and CI does not exercise them.
>
> If you are picking up M5 work, expect this document to need updates: CMake / Ninja / sccache / WinLibs pins will be stale, vcpkg's MinGW triplets may have moved, and there are almost certainly Linux-isms in the engine code (path casing, filesystem assumptions) that need cleaning up before a Windows build succeeds.
>
> If you are *not* picking up M5 work and you're reading this on Windows, **switch to Fedora** (`docs/setup/fedora.md`). Toolbox works fine under WSL2 if you must stay on a Windows host.

---

Two paths, pick one. The admin path is faster to set up; the portable path is the only option on locked-down machines.

## Path A — admin rights (recommended when available)

### 1. Install Visual Studio Build Tools

Download the Visual Studio 2022 Build Tools (~3 GB):
https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2022

In the installer, select:

- **Workloads → Desktop development with C++**
- Individual components:
  - MSVC v143 - VS 2022 C++ x64/x86 build tools (latest)
  - Windows 11 SDK (latest)
  - C++ CMake tools for Windows
  - C++ Clang Compiler for Windows (optional, but enables `clang-cl`)

### 2. Install the supporting tools

Easiest via [Scoop](https://scoop.sh/) (no admin):

```powershell
iwr -useb get.scoop.sh | iex
scoop install git ninja sccache cmake pwsh
```

Or with admin-installed Chocolatey:

```powershell
choco install -y git ninja sccache cmake pwsh
```

### 3. vcpkg

```powershell
git clone --depth 1 https://github.com/microsoft/vcpkg $HOME\vcpkg
& "$HOME\vcpkg\bootstrap-vcpkg.bat" -disableMetrics
[Environment]::SetEnvironmentVariable('VCPKG_ROOT', "$HOME\vcpkg", 'User')
[Environment]::SetEnvironmentVariable(
    'CMAKE_TOOLCHAIN_FILE',
    "$HOME\vcpkg\scripts\buildsystems\vcpkg.cmake", 'User')
```

Close and reopen the shell.

### 4. First build

```powershell
cd path\to\engine
pip install pre-commit ; pre-commit install
cmake --preset win-msvc-dbg
cmake --build --preset win-msvc-dbg
ctest --preset win-msvc-dbg
.\build\win-msvc-dbg\Debug\examples\hello_window\hello_window.exe
```

---

## Path B — no admin rights (portable)

Everything self-contained under `tools/portable/`. Nothing touches `Program Files`, the registry, or system PATH.

### Prerequisites

- PowerShell 5.1 (built-in on Windows 10/11) or pwsh 7+
- `git` on PATH (Git for Windows installs to user dir without admin)
- Outbound HTTPS to `github.com` and `objects.githubusercontent.com`

### Run the bootstrap

```powershell
cd path\to\engine
powershell -NoProfile -ExecutionPolicy Bypass -File tools\bootstrap-windows-portable.ps1
```

This downloads, into `tools/portable/`:

- **WinLibs MinGW-w64** GCC 16.1.0 (UCRT, POSIX threads, SEH) — ~500 MB
- **CMake** 4.3.3 — ~50 MB
- **Ninja** 1.13.2 — ~500 KB
- **sccache** 0.15.0 — ~5 MB
- **vcpkg** — `git clone` + bootstrap

It's idempotent; re-running skips anything already present. Pass `-Force` to re-download everything.

### Activate per shell session

```powershell
. tools\portable\activate.ps1
```

This prepends the portable bin directories to `PATH` for the current shell and sets `VCPKG_ROOT`, `CMAKE_TOOLCHAIN_FILE`, `SCCACHE_DIR`, `VCPKG_DEFAULT_BINARY_CACHE`. Nothing persists across shells — each new shell needs another `. activate.ps1`.

### First build

```powershell
cmake --preset win-mingw-dbg
cmake --build --preset win-mingw-dbg
ctest --preset win-mingw-dbg
.\build\win-mingw-dbg\examples\hello_window\hello_window.exe
```

### Manual download fallback

If `bootstrap-windows-portable.ps1` can't reach GitHub (corporate proxy / firewall), download the archives manually and drop them in `tools/portable/_cache/` with these exact filenames:

| File | URL |
|---|---|
| `winlibs-mingw-16.1.0-ucrt.zip` | https://github.com/brechtsanders/winlibs_mingw/releases/download/16.1.0posix-14.0.0-ucrt-r2/winlibs-x86_64-posix-seh-gcc-16.1.0-mingw-w64ucrt-14.0.0-r2.zip |
| `cmake-4.3.3-windows-x86_64.zip` | https://github.com/Kitware/CMake/releases/download/v4.3.3/cmake-4.3.3-windows-x86_64.zip |
| `ninja-win-1.13.2.zip` | https://github.com/ninja-build/ninja/releases/download/v1.13.2/ninja-win.zip |
| `sccache-v0.15.0-windows.tar.gz` | https://github.com/mozilla/sccache/releases/download/v0.15.0/sccache-v0.15.0-x86_64-pc-windows-msvc.tar.gz |

Then re-run the bootstrap — it'll skip the downloads and just extract.

vcpkg still requires `git clone` to GitHub; if even git is blocked, mirror it to internal git, then point `tools/portable/vcpkg` at the mirror.

---

## Troubleshooting

- **Long path errors during vcpkg build** (`The system cannot find the path specified` at depth > 260 chars). Two fixes, pick whichever is available:
  - Move the repo to a short path: `C:\dev\engine\`.
  - With admin, enable long paths once:
    ```powershell
    Set-ItemProperty -Path 'HKLM:\SYSTEM\CurrentControlSet\Control\FileSystem' `
        -Name LongPathsEnabled -Value 1
    ```
- **Slow Defender scans during build:** ask IT to exclude the repo root + `tools/portable/_vcpkg_cache/`. Without an exclusion, expect 3–5× slower clean builds.
- **vcpkg installs SDL3 from source for `x64-mingw-dynamic`:** expected. First build is 10–20 min; subsequent builds hit the binary cache.
- **`clang-tidy` not found in pre-commit on MinGW:** WinLibs doesn't ship LLVM tools. Either install LLVM separately (Scoop: `scoop install llvm`), or skip the tidy hook locally (`SKIP=clang-tidy-diff git commit ...`) and rely on CI.
