#!/usr/bin/env bash
# Runs clang-tidy only on lines changed in the current diff.
# Skips silently if compile_commands.json or clang-tidy isn't available.
set -euo pipefail

if ! command -v clang-tidy >/dev/null 2>&1; then
    echo "clang-tidy not installed; skipping." >&2
    exit 0
fi

BUILD_DIR="${BUILD_DIR:-build/linux-clang-asan}"
if [[ ! -f "$BUILD_DIR/compile_commands.json" ]]; then
    echo "no compile_commands.json at $BUILD_DIR; skipping tidy." >&2
    exit 0
fi

# Run against staged diff, restricted to passed files.
git diff -U0 --cached -- "$@" \
    | clang-tidy-diff -p1 -path "$BUILD_DIR" -quiet
