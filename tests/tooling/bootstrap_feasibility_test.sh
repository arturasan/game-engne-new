#!/usr/bin/env bash
set -Eeuo pipefail

repo_root="$(realpath "${1:?source directory is required}")"
tmp_root="$(mktemp -d)"
trap 'rm -rf "$tmp_root"' EXIT
suppress_err_trap=0
trap 'if [[ "$suppress_err_trap" != "1" ]]; then printf "FAIL: command failed at %s:%s: %s\n" "${BASH_SOURCE[0]}" "$LINENO" "$BASH_COMMAND" >&2; fi' ERR

source_root="$tmp_root/source"
mkdir -p "$source_root"
cp -R "$repo_root/tools" "$source_root/tools"
cp "$repo_root/CMakePresets.json" "$source_root/CMakePresets.json"
cp "$repo_root/vcpkg.json" "$source_root/vcpkg.json"
git -C "$source_root" init -q
source_root="$(realpath "$source_root")"

fail() {
  printf 'FAIL: %s\n' "$*" >&2
  exit 1
}

assert_file() {
  [[ -f "$1" ]] || fail "expected file '$1'"
}

assert_dir_absent() {
  [[ ! -d "$1" ]] || fail "expected directory '$1' to be absent"
}

assert_contains() {
  local file="$1"
  local pattern="$2"
  grep -Fq "$pattern" "$file" || fail "expected '$file' to contain '$pattern'"
}

assert_not_contains() {
  local file="$1"
  local pattern="$2"
  if grep -Fq "$pattern" "$file"; then
    fail "did not expect '$file' to contain '$pattern'"
  fi
}

assert_tree_not_contains() {
  local pattern="$1"
  shift
  local grep_output="$tmp_root/grep-output.txt"
  if grep -R -F -n "$pattern" "$@" >"$grep_output" 2>/dev/null; then
    cat "$grep_output" >&2
    fail "unexpected pattern '$pattern' in $*"
  fi
}

run_expect_failure() {
  local output="$1"
  shift
  suppress_err_trap=1
  set +e
  "$@" >"$output" 2>&1
  local status=$?
  set -e
  suppress_err_trap=0
  [[ $status -ne 0 ]] || fail "expected command to fail: $*"
}

run_expect_success() {
  local output="$1"
  shift
  "$@" >"$output" 2>&1
}

make_fake_vcpkg_remote() {
  local work="$1"
  git -C "$work" init -q
  git -C "$work" config user.email tooling-test@example.invalid
  git -C "$work" config user.name "Tooling Test"
  cat >"$work/bootstrap-vcpkg.sh" <<'EOF'
#!/usr/bin/env bash
set -Eeuo pipefail
mkdir -p scripts/buildsystems
: >scripts/buildsystems/vcpkg.cmake
cat >vcpkg <<'VCPKG'
#!/usr/bin/env bash
printf 'fake vcpkg\n'
VCPKG
chmod +x vcpkg
EOF
  chmod +x "$work/bootstrap-vcpkg.sh"
  git -C "$work" add bootstrap-vcpkg.sh
  git -C "$work" commit -q -m "fake vcpkg"
  git -C "$work" rev-parse HEAD
}

make_fake_required_tools() {
  local dir="$1"
  local tool
  for tool in bash realpath dirname sed head grep find readlink ln mkdir chmod rm cp cat printf basename; do
    ln -s "/usr/bin/$tool" "$dir/$tool"
  done

  for tool in git cmake ninja gcc g++ clang clang++ clangd mold sccache python3 perl vulkaninfo curl tar zip unzip; do
    cat >"$dir/$tool" <<EOF
#!/usr/bin/env bash
if [[ "$tool" == "cmake" && "\${1:-}" == "--list-presets" ]]; then
  printf 'Available configure presets:\n\n  "linux-clang-asan"\n  "linux-gcc-rel"\n'
  exit 0
fi
if [[ "$tool" == "cmake" && "\${1:-}" == "--workflow" && "\${2:-}" == "--list-presets" ]]; then
  printf 'Available workflow presets:\n\n  "check"\n'
  exit 0
fi
if [[ "$tool" == "perl" && "\${1:-}" == "-Mopen=IO" ]]; then
  exit 0
fi
if [[ "$tool" == "git" ]]; then
  exec /usr/bin/git "\$@"
fi
if [[ "\${1:-}" == "--version" ]]; then
  case "$tool" in
    cmake) printf 'cmake version 3.30.0\n' ;;
    ninja) printf '1.12.0\n' ;;
    gcc | g++) printf '%s (GCC) 16.0.0\n' "$tool" ;;
    clang | clang++) printf '%s version 20.0.0\n' "$tool" ;;
    clangd) printf 'clangd version 20.0.0\n' ;;
    *) printf '%s fake-version\n' "$tool" ;;
  esac
  exit 0
fi
exit 0
EOF
    chmod +x "$dir/$tool"
  done

  cat >"$dir/pre-commit" <<'EOF'
#!/usr/bin/env bash
set -Eeuo pipefail
if [[ "${1:-}" == "--version" ]]; then
  printf 'pre-commit fake-version\n'
  exit 0
fi
if [[ "${1:-}" == "install" ]]; then
  mkdir -p .git/hooks
  cat >.git/hooks/pre-commit <<'HOOK'
#!/usr/bin/env bash
exit 0
HOOK
  chmod +x .git/hooks/pre-commit
  exit 0
fi
exit 0
EOF
  chmod +x "$dir/pre-commit"

  cat >"$dir/pkg-config" <<'EOF'
#!/usr/bin/env bash
set -Eeuo pipefail
case "${1:-}" in
  --exists)
    exit 0
    ;;
  --modversion)
    printf '1.0.0\n'
    exit 0
    ;;
  --version)
    printf 'pkg-config fake-version\n'
    exit 0
    ;;
esac
exit 0
EOF
  chmod +x "$dir/pkg-config"

  cat >"$dir/rpm" <<'EOF'
#!/usr/bin/env bash
set -Eeuo pipefail
if [[ "${1:-}" == "-q" && -n "${2:-}" ]]; then
  printf '%s-1.0.0-1.fc.test.x86_64\n' "$2"
  exit 0
fi
exit 1
EOF
  chmod +x "$dir/rpm"
}

set_fake_tool_version() {
  local dir="$1"
  local tool="$2"
  local version_output="$3"

  cat >"$dir/$tool" <<EOF
#!/usr/bin/env bash
if [[ "\${1:-}" == "--version" ]]; then
  printf '%s\n' "$version_output"
  exit 0
fi
exit 0
EOF
  chmod +x "$dir/$tool"
}

make_fixture_source() {
  local target="$1"
  mkdir -p "$target"
  cp -R "$repo_root/tools" "$target/tools"
  cp "$repo_root/CMakePresets.json" "$target/CMakePresets.json"
  cp "$repo_root/vcpkg.json" "$target/vcpkg.json"
  git -C "$target" init -q
}

vcpkg_remote="$tmp_root/fake-vcpkg"
mkdir -p "$vcpkg_remote"
vcpkg_baseline="$(make_fake_vcpkg_remote "$vcpkg_remote")"

fake_bin="$tmp_root/fake-bin"
fake_home="$tmp_root/home"
mkdir -p "$fake_bin" "$fake_home"
make_fake_required_tools "$fake_bin"

common_env=(
  HOME="$fake_home"
  PATH="$fake_bin"
  GE_DEV_VCPKG_REMOTE="$vcpkg_remote"
  GE_DEV_VCPKG_BASELINE="$vcpkg_baseline"
)

run_expect_failure "$tmp_root/check-before.out" env "${common_env[@]}" bash "$source_root/tools/dev" bootstrap --check
assert_contains "$tmp_root/check-before.out" "Development model: Fedora host-native"
assert_contains "$tmp_root/check-before.out" "FAIL vcpkg.root missing"
assert_contains "$tmp_root/check-before.out" "ACTION bootstrap would clone $vcpkg_remote at $vcpkg_baseline"
[[ ! -e "$source_root/.cache/dev/vcpkg" ]] || fail "--check created vcpkg"
[[ ! -e "$source_root/compile_commands.json" ]] || fail "--check created compile_commands.json"

missing_tool_root="$tmp_root/missing-tool-source"
make_fixture_source "$missing_tool_root"
missing_tool_bin="$tmp_root/missing-tool-bin"
mkdir -p "$missing_tool_bin"
make_fake_required_tools "$missing_tool_bin"
rm "$missing_tool_bin/curl"
run_expect_failure "$tmp_root/missing-tool.out" env \
  HOME="$fake_home" \
  PATH="$missing_tool_bin" \
  GE_DEV_VCPKG_REMOTE="$vcpkg_remote" \
  GE_DEV_VCPKG_BASELINE="$vcpkg_baseline" \
  bash "$missing_tool_root/tools/dev" bootstrap
assert_contains "$tmp_root/missing-tool.out" "FAIL host.tool.curl missing"
assert_contains "$tmp_root/missing-tool.out" "FAIL bootstrap.preflight prerequisites failed; generated state was not modified"
assert_dir_absent "$missing_tool_root/.cache/dev/vcpkg"
[[ ! -e "$missing_tool_root/compile_commands.json" ]] || fail "preflight failure created compile_commands.json"

missing_pkg_root="$tmp_root/missing-pkg-source"
make_fixture_source "$missing_pkg_root"
missing_pkg_bin="$tmp_root/missing-pkg-bin"
mkdir -p "$missing_pkg_bin"
make_fake_required_tools "$missing_pkg_bin"
cat >"$missing_pkg_bin/pkg-config" <<'EOF'
#!/usr/bin/env bash
set -Eeuo pipefail
case "${1:-}" in
  --exists)
    [[ "${2:-}" != "x11" ]]
    exit $?
    ;;
  --modversion)
    printf '1.0.0\n'
    exit 0
    ;;
  --version)
    printf 'pkg-config fake-version\n'
    exit 0
    ;;
esac
exit 0
EOF
chmod +x "$missing_pkg_bin/pkg-config"
run_expect_failure "$tmp_root/missing-pkg.out" env \
  HOME="$fake_home" \
  PATH="$missing_pkg_bin" \
  GE_DEV_VCPKG_REMOTE="$vcpkg_remote" \
  GE_DEV_VCPKG_BASELINE="$vcpkg_baseline" \
  bash "$missing_pkg_root/tools/dev" bootstrap
assert_contains "$tmp_root/missing-pkg.out" "FAIL host.pkg_config.x11 missing"
assert_contains "$tmp_root/missing-pkg.out" "FAIL bootstrap.preflight prerequisites failed; generated state was not modified"
assert_dir_absent "$missing_pkg_root/.cache/dev/vcpkg"
[[ ! -e "$missing_pkg_root/compile_commands.json" ]] || fail "preflight failure created compile_commands.json"

missing_vulkan_root="$tmp_root/missing-vulkan-source"
make_fixture_source "$missing_vulkan_root"
missing_vulkan_bin="$tmp_root/missing-vulkan-bin"
mkdir -p "$missing_vulkan_bin"
make_fake_required_tools "$missing_vulkan_bin"
cat >"$missing_vulkan_bin/pkg-config" <<'EOF'
#!/usr/bin/env bash
set -Eeuo pipefail
case "${1:-}" in
  --exists)
    [[ "${2:-}" != "vulkan" ]]
    exit $?
    ;;
  --modversion)
    printf '1.0.0\n'
    exit 0
    ;;
  --version)
    printf 'pkg-config fake-version\n'
    exit 0
    ;;
esac
exit 0
EOF
chmod +x "$missing_vulkan_bin/pkg-config"
run_expect_failure "$tmp_root/missing-vulkan.out" env \
  HOME="$fake_home" \
  PATH="$missing_vulkan_bin" \
  GE_DEV_VCPKG_REMOTE="$vcpkg_remote" \
  GE_DEV_VCPKG_BASELINE="$vcpkg_baseline" \
  bash "$missing_vulkan_root/tools/dev" bootstrap
assert_contains "$tmp_root/missing-vulkan.out" "FAIL host.pkg_config.vulkan missing"
assert_contains "$tmp_root/missing-vulkan.out" "FAIL bootstrap.preflight prerequisites failed; generated state was not modified"
assert_dir_absent "$missing_vulkan_root/.cache/dev/vcpkg"
[[ ! -e "$missing_vulkan_root/compile_commands.json" ]] || fail "missing Vulkan pkg-config created compile_commands.json"

old_version_root="$tmp_root/old-version-source"
make_fixture_source "$old_version_root"
old_version_bin="$tmp_root/old-version-bin"
mkdir -p "$old_version_bin"
make_fake_required_tools "$old_version_bin"
set_fake_tool_version "$old_version_bin" cmake "cmake version 3.29.9"
run_expect_failure "$tmp_root/old-version.out" env \
  HOME="$fake_home" \
  PATH="$old_version_bin" \
  GE_DEV_VCPKG_REMOTE="$vcpkg_remote" \
  GE_DEV_VCPKG_BASELINE="$vcpkg_baseline" \
  bash "$old_version_root/tools/dev" bootstrap
assert_contains "$tmp_root/old-version.out" "FAIL host.tool.cmake version_too_old expected>=3.30 observed=3.29.9"
assert_dir_absent "$old_version_root/.cache/dev/vcpkg"
[[ ! -e "$old_version_root/compile_commands.json" ]] || fail "old version created compile_commands.json"

malformed_version_root="$tmp_root/malformed-version-source"
make_fixture_source "$malformed_version_root"
malformed_version_bin="$tmp_root/malformed-version-bin"
mkdir -p "$malformed_version_bin"
make_fake_required_tools "$malformed_version_bin"
set_fake_tool_version "$malformed_version_bin" ninja "ninja no-version"
run_expect_failure "$tmp_root/malformed-version.out" env \
  HOME="$fake_home" \
  PATH="$malformed_version_bin" \
  GE_DEV_VCPKG_REMOTE="$vcpkg_remote" \
  GE_DEV_VCPKG_BASELINE="$vcpkg_baseline" \
  bash "$malformed_version_root/tools/dev" bootstrap
assert_contains "$tmp_root/malformed-version.out" "FAIL host.tool.ninja version_unparseable expected>=1.12 observed=ninja no-version"
assert_dir_absent "$malformed_version_root/.cache/dev/vcpkg"
[[ ! -e "$malformed_version_root/compile_commands.json" ]] || fail "malformed version created compile_commands.json"

newer_version_root="$tmp_root/newer-version-source"
make_fixture_source "$newer_version_root"
newer_version_bin="$tmp_root/newer-version-bin"
mkdir -p "$newer_version_bin"
make_fake_required_tools "$newer_version_bin"
set_fake_tool_version "$newer_version_bin" clang "clang version 21.2.0"
run_expect_success "$tmp_root/newer-version.out" env \
  HOME="$fake_home" \
  PATH="$newer_version_bin" \
  GE_DEV_VCPKG_REMOTE="$vcpkg_remote" \
  GE_DEV_VCPKG_BASELINE="$vcpkg_baseline" \
  bash "$newer_version_root/tools/dev" bootstrap
assert_contains "$tmp_root/newer-version.out" "PASS host.tool.clang"
assert_contains "$tmp_root/newer-version.out" "observed=21.2.0"

run_expect_success "$tmp_root/bootstrap.out" env "${common_env[@]}" bash "$source_root/tools/dev" bootstrap
run_expect_success "$tmp_root/bootstrap-second.out" env "${common_env[@]}" bash "$source_root/tools/dev" bootstrap
run_expect_success "$tmp_root/check-after.out" env "${common_env[@]}" bash "$source_root/tools/dev" bootstrap --check

vcpkg_root="$source_root/.cache/dev/vcpkg"
assert_file "$source_root/.git/hooks/pre-commit"
[[ -L "$source_root/compile_commands.json" ]] || fail "compile_commands.json is not a symlink"
[[ "$(readlink "$source_root/compile_commands.json")" == "build/linux-clang-asan/compile_commands.json" ]] ||
  fail "compile_commands.json target mismatch"
[[ "$(git -C "$vcpkg_root" rev-parse HEAD)" == "$vcpkg_baseline" ]] || fail "vcpkg revision mismatch"
assert_file "$vcpkg_root/scripts/buildsystems/vcpkg.cmake"
[[ -x "$vcpkg_root/vcpkg" ]] || fail "vcpkg binary is not executable"
assert_contains "$source_root/CMakePresets.json" '"toolchainFile": "${sourceDir}/.cache/dev/vcpkg/scripts/buildsystems/vcpkg.cmake"'
assert_contains "$tmp_root/bootstrap-second.out" "PASS vcpkg.revision $vcpkg_baseline"
assert_contains "$tmp_root/check-after.out" "PASS cmake.preset linux-clang-asan"
assert_contains "$tmp_root/check-after.out" "PASS cmake.preset linux-gcc-rel"
assert_contains "$tmp_root/check-after.out" "PASS cmake.workflow check"
assert_contains "$tmp_root/check-after.out" "PASS cmake.vcpkg_toolchain repo-local"
assert_contains "$tmp_root/check-after.out" "PASS clangd.compile_commands_link"
assert_contains "$tmp_root/check-after.out" "WARN clangd.compile_commands_target missing until linux-clang-asan is configured"

archive_root="$tmp_root/source-archive"
cp -a "$source_root" "$archive_root"
rm -rf "$archive_root/.git" "$archive_root/.cache" "$archive_root/build" "$archive_root/compile_commands.json"
env \
  GE_DEV_CANONICAL_REPO="$archive_root" \
  GE_DEV_VCPKG_REMOTE="$vcpkg_remote" \
  GE_DEV_VCPKG_BASELINE="$vcpkg_baseline" \
  PATH="$fake_bin" \
  bash "$archive_root/tools/dev" bootstrap >"$tmp_root/archive-bootstrap.out"
assert_contains "$tmp_root/archive-bootstrap.out" "SKIP pre-commit.hook no .git directory"
[[ ! -e "$archive_root/.git" ]] || fail "bootstrap created .git in archive checkout"

run_expect_failure "$tmp_root/clean-removed.out" env PATH="$fake_bin" bash "$source_root/tools/dev" clean-build linux-clang-asan
assert_contains "$tmp_root/clean-removed.out" "Unknown command 'clean-build'"

[[ ! -e "$source_root/CMakeUserPresets.json" ]] || fail "bootstrap generated CMakeUserPresets.json"
[[ ! -e "$source_root/.idea" ]] || fail "bootstrap generated .idea"
[[ ! -e "$source_root/.cache/dev/bin/open-clion" ]] || fail "bootstrap generated CLion wrapper"
[[ ! -e "$fake_home/.local/share/applications/game-engine-clion.desktop" ]] ||
  fail "bootstrap generated desktop launcher"

if [[ -n "${USER:-}" ]]; then
  assert_tree_not_contains "/home/$USER/" "$repo_root/tools"
  assert_tree_not_contains "/var/home/$USER/" "$repo_root/tools"
fi
assert_tree_not_contains "$repo_root" "$repo_root/tools"
assert_tree_not_contains "CLion202" "$repo_root/tools"
assert_tree_not_contains "open-clion" "$repo_root/tools"
assert_tree_not_contains "game-engine-clion.desktop" "$repo_root/tools"

printf 'bootstrap tooling tests passed\n'
