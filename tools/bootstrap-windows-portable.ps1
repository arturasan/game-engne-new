# bootstrap-windows-portable.ps1
#
# Set up a fully portable, no-admin Windows build environment for the engine
# under tools/portable/. Downloads WinLibs MinGW-w64 GCC, CMake, Ninja, sccache,
# and clones+bootstraps vcpkg. Versions are pinned via the table below — bump
# them when you want to update a tool. URL patterns are stable; if a tool's
# pattern changes you'll need to edit the corresponding download URL too.
#
# Usage (from repo root, PowerShell 5.1+ or pwsh):
#     powershell -NoProfile -ExecutionPolicy Bypass -File tools/bootstrap-windows-portable.ps1
#     . tools/portable/activate.ps1   # adds everything to PATH for this session
#     cmake --preset win-mingw-dbg
#     cmake --build --preset win-mingw-dbg
#     ctest --preset win-mingw-dbg
#
# Re-running the script is safe; it skips downloads for tools already present.
# To force a refresh, pass -Force or delete tools/portable/<toolname>/.

[CmdletBinding()]
param(
    [string] $Root,
    [switch] $Force
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

# $PSScriptRoot can be empty when the script is invoked via certain launchers
# (e.g. powershell.exe -File from cmd/bash); fall back to $MyInvocation.
$script_dir = $PSScriptRoot
if (-not $script_dir) { $script_dir = Split-Path -Parent $MyInvocation.MyCommand.Path }
if (-not $Root) { $Root = Join-Path $script_dir 'portable' }

# ----------------------------------------------------------------------------
# Pinned versions and URLs. When you bump a version, also update the URL.
# Find latest:
#   WinLibs: https://github.com/brechtsanders/winlibs_mingw/releases
#   CMake:   https://github.com/Kitware/CMake/releases
#   Ninja:   https://github.com/ninja-build/ninja/releases
#   sccache: https://github.com/mozilla/sccache/releases
# ----------------------------------------------------------------------------
$tools = @(
    @{
        Name       = 'mingw'
        Url        = 'https://github.com/brechtsanders/winlibs_mingw/releases/download/16.1.0posix-14.0.0-ucrt-r2/winlibs-x86_64-posix-seh-gcc-16.1.0-mingw-w64ucrt-14.0.0-r2.zip'
        Archive    = 'winlibs-mingw-16.1.0-ucrt.zip'
        Marker     = 'bin\g++.exe'
        # zip contains a single top-level 'mingw64' directory.
        InnerDir   = 'mingw64'
    },
    @{
        Name       = 'cmake'
        Url        = 'https://github.com/Kitware/CMake/releases/download/v4.3.3/cmake-4.3.3-windows-x86_64.zip'
        Archive    = 'cmake-4.3.3-windows-x86_64.zip'
        Marker     = 'bin\cmake.exe'
        InnerDir   = 'cmake-4.3.3-windows-x86_64'
    },
    @{
        Name       = 'ninja'
        Url        = 'https://github.com/ninja-build/ninja/releases/download/v1.13.2/ninja-win.zip'
        Archive    = 'ninja-win-1.13.2.zip'
        Marker     = 'ninja.exe'
        InnerDir   = $null  # extracts ninja.exe directly into the target dir
    },
    @{
        Name       = 'sccache'
        Url        = 'https://github.com/mozilla/sccache/releases/download/v0.15.0/sccache-v0.15.0-x86_64-pc-windows-msvc.tar.gz'
        Archive    = 'sccache-v0.15.0-windows.tar.gz'
        Marker     = 'sccache.exe'
        InnerDir   = 'sccache-v0.15.0-x86_64-pc-windows-msvc'
    }
)

function Ensure-Dir { param([string] $P) if (-not (Test-Path $P)) { New-Item -ItemType Directory -Path $P | Out-Null } }

function Download-File {
    param([string] $Url, [string] $Dest)
    Write-Host "  downloading $([System.IO.Path]::GetFileName($Dest))..."
    # curl.exe ships with Windows 10 1804+ and is dramatically more reliable
    # for large files than Invoke-WebRequest, which buffers the full response
    # in memory and routinely drops 500MB+ transfers on PS 5.1.
    $tmp = "$Dest.part"
    if (Test-Path $tmp) { Remove-Item $tmp -Force }
    & curl.exe --fail --location --silent --show-error --retry 3 `
        --connect-timeout 30 --output $tmp $Url
    if ($LASTEXITCODE -ne 0) {
        if (Test-Path $tmp) { Remove-Item $tmp -Force }
        throw "download failed for $Url (curl exit $LASTEXITCODE)`n  if the version moved, update the URL at the top of this script."
    }
    Move-Item -Path $tmp -Destination $Dest -Force
}

function Expand-Any {
    # zip via Expand-Archive, tar.gz / tar.xz via built-in tar.exe (Win10 1803+).
    param([string] $Archive, [string] $Dest)
    Ensure-Dir $Dest
    switch -Regex ($Archive) {
        '\.zip$'            { Expand-Archive -Path $Archive -DestinationPath $Dest -Force }
        '\.tar\.gz$|\.tgz$' { & tar -xzf $Archive -C $Dest; if ($LASTEXITCODE -ne 0) { throw "tar -xzf failed for $Archive" } }
        '\.tar\.xz$'        { & tar -xJf $Archive -C $Dest; if ($LASTEXITCODE -ne 0) { throw "tar -xJf failed for $Archive" } }
        default             { throw "unknown archive type: $Archive" }
    }
}

function Install-Tool {
    param([hashtable] $T, [string] $RootDir, [string] $Cache, [switch] $Force)
    $dest = Join-Path $RootDir $T.Name
    if ((-not $Force) -and (Test-Path (Join-Path $dest $T.Marker))) {
        Write-Host "  $($T.Name) already present, skipping."
        return
    }
    $archive = Join-Path $Cache $T.Archive
    if (-not (Test-Path $archive)) { Download-File -Url $T.Url -Dest $archive }
    if (Test-Path $dest) { Remove-Item $dest -Recurse -Force }
    if ($T.InnerDir) {
        $tmp = Join-Path $Cache "$($T.Name)_extract"
        if (Test-Path $tmp) { Remove-Item $tmp -Recurse -Force }
        Expand-Any -Archive $archive -Dest $tmp
        $inner = Join-Path $tmp $T.InnerDir
        if (-not (Test-Path $inner)) {
            $found = Get-ChildItem $tmp | Where-Object PSIsContainer | Select-Object -First 1
            if (-not $found) { throw "no inner directory found in $archive" }
            $inner = $found.FullName
        }
        Move-Item -Path $inner -Destination $dest
        Remove-Item $tmp -Recurse -Force
    } else {
        Ensure-Dir $dest
        Expand-Any -Archive $archive -Dest $dest
    }
    if (-not (Test-Path (Join-Path $dest $T.Marker))) {
        throw "$($T.Name) installed but marker '$($T.Marker)' missing"
    }
}

Ensure-Dir $Root
$cache = Join-Path $Root '_cache'
Ensure-Dir $cache

$i = 0
foreach ($t in $tools) {
    $i++
    Write-Host "[$i/$($tools.Count + 1)] $($t.Name)..."
    Install-Tool -T $t -RootDir $Root -Cache $cache -Force:$Force
}

# vcpkg: git clone + bootstrap (no archive download).
$vcpkg_dir = Join-Path $Root 'vcpkg'
Write-Host "[$($tools.Count + 1)/$($tools.Count + 1)] vcpkg..."
if ($Force -or -not (Test-Path (Join-Path $vcpkg_dir 'vcpkg.exe'))) {
    if (-not (Test-Path $vcpkg_dir)) {
        & git clone --depth 1 https://github.com/microsoft/vcpkg $vcpkg_dir
        if ($LASTEXITCODE -ne 0) { throw 'git clone vcpkg failed (is git on PATH?)' }
    }
    & (Join-Path $vcpkg_dir 'bootstrap-vcpkg.bat') -disableMetrics
    if ($LASTEXITCODE -ne 0) { throw 'vcpkg bootstrap failed' }
} else {
    Write-Host '  vcpkg already present, skipping.'
}

# --- Emit activate.ps1 -------------------------------------------------------
$activate = @"
# Generated by bootstrap-windows-portable.ps1 — dot-source into a shell:
#     . tools/portable/activate.ps1
`$root = `$PSScriptRoot
`$env:Path = (@(
    (Join-Path `$root 'mingw\bin'),
    (Join-Path `$root 'cmake\bin'),
    (Join-Path `$root 'ninja'),
    (Join-Path `$root 'sccache'),
    (Join-Path `$root 'vcpkg')
) -join ';') + ';' + `$env:Path
`$env:VCPKG_ROOT               = Join-Path `$root 'vcpkg'
`$env:CMAKE_TOOLCHAIN_FILE     = Join-Path `$root 'vcpkg\scripts\buildsystems\vcpkg.cmake'
`$env:SCCACHE_DIR              = Join-Path `$root '_sccache_cache'
`$env:VCPKG_DEFAULT_BINARY_CACHE = Join-Path `$root '_vcpkg_cache'
New-Item -ItemType Directory -Force -Path `$env:SCCACHE_DIR, `$env:VCPKG_DEFAULT_BINARY_CACHE | Out-Null
Write-Host 'engine portable env active:'
Write-Host "  gcc:    `$((Get-Command g++.exe).Source)"
Write-Host "  cmake:  `$((Get-Command cmake.exe).Source)"
Write-Host "  ninja:  `$((Get-Command ninja.exe).Source)"
Write-Host "  vcpkg:  `$env:VCPKG_ROOT"
"@
Set-Content -Path (Join-Path $Root 'activate.ps1') -Value $activate -Encoding UTF8

Write-Host ''
Write-Host 'done. next steps:'
Write-Host '    . tools/portable/activate.ps1'
Write-Host '    cmake --preset win-mingw-dbg'
Write-Host '    cmake --build --preset win-mingw-dbg'
Write-Host '    ctest --preset win-mingw-dbg'
