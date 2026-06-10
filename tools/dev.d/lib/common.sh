#!/usr/bin/env bash

ge_dev_die() {
  printf 'ERROR: %s\n' "$*" >&2
  exit 1
}

ge_dev_info() {
  printf '%s\n' "$*"
}

ge_dev_warn() {
  printf 'WARN: %s\n' "$*" >&2
}

ge_dev_default_vcpkg_root() {
  local repo_root="$1"
  printf '%s/.cache/dev/vcpkg\n' "$repo_root"
}

ge_dev_default_compile_commands_link() {
  local repo_root="$1"
  printf '%s/compile_commands.json\n' "$repo_root"
}

ge_dev_vcpkg_baseline() {
  local repo_root="$1"
  if [[ -n "${GE_DEV_VCPKG_BASELINE:-}" ]]; then
    printf '%s\n' "$GE_DEV_VCPKG_BASELINE"
    return 0
  fi
  sed -n 's/.*"builtin-baseline"[[:space:]]*:[[:space:]]*"\([0-9a-fA-F]\{40\}\)".*/\1/p' "$repo_root/vcpkg.json" | head -n 1
}

ge_dev_required_tools() {
  printf '%s\n' \
    git \
    cmake \
    ninja \
    gcc \
    g++ \
    clang \
    clang++ \
    clangd \
    mold \
    sccache \
    python3 \
    perl \
    pre-commit \
    vulkaninfo \
    pkg-config \
    curl \
    tar \
    zip \
    unzip
}

ge_dev_version_requirements() {
  printf '%s\n' \
    'cmake|3.30|cmake_minimum_required(VERSION 3.30)' \
    'ninja|1.12|documented Fedora baseline' \
    'gcc|16.0|ADR 0002 compiler floor' \
    'g++|16.0|ADR 0002 compiler floor' \
    'clang|20.0|ADR 0002 compiler floor' \
    'clang++|20.0|ADR 0002 compiler floor' \
    'clangd|20.0|ADR 0002 compiler floor'
}

ge_dev_extract_version() {
  grep -Eo '[0-9]+(\.[0-9]+){1,2}' | head -n 1
}

ge_dev_version_ge() {
  local observed="$1"
  local minimum="$2"
  local observed_part minimum_part index
  local -a observed_parts minimum_parts
  IFS=. read -r -a observed_parts <<<"$observed"
  IFS=. read -r -a minimum_parts <<<"$minimum"

  for index in 0 1 2; do
    observed_part="${observed_parts[$index]:-0}"
    minimum_part="${minimum_parts[$index]:-0}"
    if ((10#$observed_part > 10#$minimum_part)); then
      return 0
    fi
    if ((10#$observed_part < 10#$minimum_part)); then
      return 1
    fi
  done
  return 0
}

ge_dev_minimum_for_tool() {
  local requested="$1"
  local tool minimum rationale
  while IFS='|' read -r tool minimum rationale; do
    if [[ "$tool" == "$requested" ]]; then
      printf '%s|%s\n' "$minimum" "$rationale"
      return 0
    fi
  done < <(ge_dev_version_requirements)
  return 1
}

ge_dev_required_pkg_config_modules() {
  printf '%s\n' \
    x11 \
    xcursor \
    xrandr \
    xi \
    xtst \
    xinerama \
    wayland-client \
    wayland-protocols \
    xkbcommon \
    vulkan \
    gl \
    egl \
    alsa \
    libpulse
}

ge_dev_required_fedora_packages() {
  printf '%s\n' \
    autoconf \
    autoconf-archive \
    automake \
    libtool
}

ge_dev_check_host_tools() {
  local status=0 tool path version observed minimum_rationale minimum rationale
  while IFS= read -r tool; do
    [[ -n "$tool" ]] || continue
    path="$(command -v "$tool" 2>/dev/null || true)"
    if [[ -z "$path" ]]; then
      ge_dev_info "FAIL host.tool.$tool missing"
      ge_dev_info "ACTION install Fedora package providing '$tool'; see docs/setup/fedora.md"
      status=1
      continue
    fi
    version="$("$tool" --version 2>/dev/null | head -n 1 || true)"
    if minimum_rationale="$(ge_dev_minimum_for_tool "$tool")"; then
      minimum="${minimum_rationale%%|*}"
      rationale="${minimum_rationale#*|}"
      observed="$(printf '%s\n' "$version" | ge_dev_extract_version)"
      if [[ -z "$observed" ]]; then
        ge_dev_info "FAIL host.tool.$tool version_unparseable expected>=$minimum observed=${version:-missing}"
        ge_dev_info "ACTION install a Fedora package satisfying '$tool >= $minimum' ($rationale); see docs/setup/fedora.md"
        status=1
        continue
      fi
      if ! ge_dev_version_ge "$observed" "$minimum"; then
        ge_dev_info "FAIL host.tool.$tool version_too_old expected>=$minimum observed=$observed"
        ge_dev_info "ACTION install a Fedora package satisfying '$tool >= $minimum' ($rationale); see docs/setup/fedora.md"
        status=1
        continue
      fi
      ge_dev_info "PASS host.tool.$tool $path | expected>=$minimum observed=$observed | $version"
    else
      ge_dev_info "PASS host.tool.$tool $path${version:+ | $version}"
    fi
  done < <(ge_dev_required_tools)
  return "$status"
}

ge_dev_preflight() {
  local repo_root="$1"
  local status=0

  ge_dev_check_host_tools || status=1
  ge_dev_check_fedora_packages || status=1
  ge_dev_check_perl_modules || status=1
  ge_dev_check_pkg_config_modules || status=1
  ge_dev_check_cmake_presets "$repo_root" || status=1

  return "$status"
}

ge_dev_check_fedora_packages() {
  command -v rpm >/dev/null 2>&1 || return 0

  local status=0 package
  while IFS= read -r package; do
    [[ -n "$package" ]] || continue
    if rpm -q "$package" >/dev/null 2>&1; then
      ge_dev_info "PASS host.rpm.$package $(rpm -q "$package")"
    else
      ge_dev_info "FAIL host.rpm.$package missing"
      ge_dev_info "ACTION install Fedora package: sudo dnf install -y $package"
      status=1
    fi
  done < <(ge_dev_required_fedora_packages)
  return "$status"
}

ge_dev_check_pkg_config_modules() {
  local status=0 module
  while IFS= read -r module; do
    [[ -n "$module" ]] || continue
    if pkg-config --exists "$module"; then
      ge_dev_info "PASS host.pkg_config.$module $(pkg-config --modversion "$module" 2>/dev/null || true)"
    else
      ge_dev_info "FAIL host.pkg_config.$module missing"
      ge_dev_info "ACTION install Fedora development package for pkg-config module '$module'; see docs/setup/fedora.md"
      status=1
    fi
  done < <(ge_dev_required_pkg_config_modules)
  return "$status"
}

ge_dev_check_perl_modules() {
  if perl -Mopen=IO -e 'exit 0' >/dev/null 2>&1; then
    ge_dev_info "PASS host.perl_module.open"
    return 0
  fi

  ge_dev_info "FAIL host.perl_module.open missing"
  ge_dev_info "ACTION install Fedora package providing Perl open.pm, for example: sudo dnf install -y perl-open"
  return 1
}

ge_dev_pre_commit_hook() {
  local repo_root="$1"
  printf '%s/.git/hooks/pre-commit\n' "$repo_root"
}

ge_dev_install_pre_commit_hook() {
  local repo_root="$1"
  local mode="$2"
  local hook
  hook="$(ge_dev_pre_commit_hook "$repo_root")"

  if [[ ! -d "$repo_root/.git" ]]; then
    ge_dev_info "SKIP pre-commit.hook no .git directory"
    return 0
  fi

  if [[ "$mode" == "check" ]]; then
    if [[ -x "$hook" ]]; then
      ge_dev_info "PASS pre-commit.hook $hook"
    else
      ge_dev_info "FAIL pre-commit.hook missing"
      ge_dev_info "ACTION bootstrap would run: pre-commit install"
      return 1
    fi
    return 0
  fi

  command -v pre-commit >/dev/null 2>&1 ||
    ge_dev_die "pre-commit is missing. Install it on the host, then rerun ./tools/dev bootstrap."

  (cd "$repo_root" && PRE_COMMIT_HOME="$repo_root/.cache/dev/pre-commit" pre-commit install)
  [[ -x "$hook" ]] || ge_dev_die "pre-commit install did not create executable hook '$hook'."
  ge_dev_info "PASS pre-commit.hook $hook"
}

ge_dev_bootstrap_vcpkg() {
  local repo_root="$1"
  local vcpkg_root="$2"
  local mode="$3"
  local baseline remote current
  baseline="$(ge_dev_vcpkg_baseline "$repo_root")"
  remote="${GE_DEV_VCPKG_REMOTE:-https://github.com/microsoft/vcpkg.git}"
  [[ -n "$baseline" ]] || ge_dev_die "Could not read builtin-baseline from '$repo_root/vcpkg.json'."

  if [[ "$mode" == "check" ]]; then
    if [[ ! -d "$vcpkg_root/.git" ]]; then
      ge_dev_info "FAIL vcpkg.root missing $vcpkg_root"
      ge_dev_info "ACTION bootstrap would clone $remote at $baseline"
      return 1
    fi

    current="$(git -C "$vcpkg_root" rev-parse HEAD 2>/dev/null || true)"
    if [[ "$current" == "$baseline" ]]; then
      ge_dev_info "PASS vcpkg.revision $current"
    else
      ge_dev_info "FAIL vcpkg.revision expected=$baseline observed=${current:-missing}"
      return 1
    fi

    if [[ -f "$vcpkg_root/scripts/buildsystems/vcpkg.cmake" ]]; then
      ge_dev_info "PASS vcpkg.toolchain $vcpkg_root/scripts/buildsystems/vcpkg.cmake"
    else
      ge_dev_info "FAIL vcpkg.toolchain missing"
      return 1
    fi

    if [[ -x "$vcpkg_root/vcpkg" ]]; then
      ge_dev_info "PASS vcpkg.binary $vcpkg_root/vcpkg"
    else
      ge_dev_info "FAIL vcpkg.binary missing"
      return 1
    fi
    return 0
  fi

  mkdir -p "$(dirname "$vcpkg_root")"
  if [[ ! -d "$vcpkg_root/.git" ]]; then
    ge_dev_info "Cloning vcpkg into '$vcpkg_root'."
    git clone --filter=blob:none --no-checkout "$remote" "$vcpkg_root" || return 1
  fi

  git -C "$vcpkg_root" fetch --depth 1 origin "$baseline" || return 1
  git -C "$vcpkg_root" checkout --detach "$baseline" || return 1

  if [[ ! -f "$vcpkg_root/scripts/buildsystems/vcpkg.cmake" || ! -x "$vcpkg_root/vcpkg" ]]; then
    ge_dev_info "Bootstrapping vcpkg with metrics disabled."
    (cd "$vcpkg_root" && ./bootstrap-vcpkg.sh -disableMetrics) || return 1
  fi

  ge_dev_info "PASS vcpkg.revision $(git -C "$vcpkg_root" rev-parse HEAD)"
  ge_dev_info "PASS vcpkg.toolchain $vcpkg_root/scripts/buildsystems/vcpkg.cmake"
  ge_dev_info "PASS vcpkg.binary $vcpkg_root/vcpkg"
}

ge_dev_create_compile_commands_link() {
  local repo_root="$1"
  local link_path="$2"
  local target="build/linux-clang-asan/compile_commands.json"

  if [[ -L "$link_path" ]]; then
    ln -sfn "$target" "$link_path"
    ge_dev_info "PASS clangd.compile_commands_link $link_path -> $target"
    return 0
  fi

  if [[ -e "$link_path" ]]; then
    ge_dev_die "'$link_path' exists and is not a symlink. Move it aside before rerunning bootstrap."
  fi

  ln -s "$target" "$link_path"
  ge_dev_info "PASS clangd.compile_commands_link $link_path -> $target"
}

ge_dev_check_compile_commands_link() {
  local repo_root="$1"
  local link_path="$2"
  local target="build/linux-clang-asan/compile_commands.json"

  if [[ -L "$link_path" && "$(readlink "$link_path")" == "$target" ]]; then
    ge_dev_info "PASS clangd.compile_commands_link $link_path -> $target"
  else
    ge_dev_info "FAIL clangd.compile_commands_link missing_or_wrong"
    ge_dev_info "ACTION bootstrap would symlink $link_path -> $target"
    return 1
  fi

  if [[ -f "$repo_root/$target" ]]; then
    ge_dev_info "PASS clangd.compile_commands_target $repo_root/$target"
  else
    ge_dev_info "WARN clangd.compile_commands_target missing until linux-clang-asan is configured"
  fi
}

ge_dev_detect_stale_compiler_caches() {
  local repo_root="$1"
  local found=0
  while IFS= read -r cache; do
    [[ -n "$cache" ]] || continue
    if grep -Eq '/usr/lib64/ccache/(cc|c\+\+|gcc|g\+\+|clang|clang\+\+)' "$cache"; then
      ge_dev_warn "stale ccache compiler wrapper recorded in $cache"
      ge_dev_warn "cleanup by removing the stale build directory: build/$(basename "$(dirname "$cache")")"
      found=1
    fi
  done < <(find "$repo_root/build" -path '*/CMakeCache.txt' -type f 2>/dev/null || true)
  return "$found"
}

ge_dev_check_cmake_presets() {
  local repo_root="$1"
  local status=0

  if (cd "$repo_root" && cmake --list-presets) | grep -Fq '"linux-clang-asan"'; then
    ge_dev_info "PASS cmake.preset linux-clang-asan"
  else
    ge_dev_info "FAIL cmake.preset linux-clang-asan missing"
    status=1
  fi

  if (cd "$repo_root" && cmake --list-presets) | grep -Fq '"linux-gcc-rel"'; then
    ge_dev_info "PASS cmake.preset linux-gcc-rel"
  else
    ge_dev_info "FAIL cmake.preset linux-gcc-rel missing"
    status=1
  fi

  if (cd "$repo_root" && cmake --workflow --list-presets) | grep -Fq '"check"'; then
    ge_dev_info "PASS cmake.workflow check"
  else
    ge_dev_info "FAIL cmake.workflow check missing"
    status=1
  fi

  if grep -Fq '"toolchainFile": "${sourceDir}/.cache/dev/vcpkg/scripts/buildsystems/vcpkg.cmake"' "$repo_root/CMakePresets.json"; then
    ge_dev_info "PASS cmake.vcpkg_toolchain repo-local"
  else
    ge_dev_info "FAIL cmake.vcpkg_toolchain not repo-local"
    status=1
  fi

  return "$status"
}

ge_dev_bootstrap() {
  local repo_root="$1"
  local mode="$2"
  local canonical_repo
  canonical_repo="$(realpath "$repo_root")"

  local vcpkg_root compile_commands_link status=0
  vcpkg_root="${GE_DEV_VCPKG_ROOT:-$(ge_dev_default_vcpkg_root "$canonical_repo")}"
  compile_commands_link="${GE_DEV_COMPILE_COMMANDS_LINK:-$(ge_dev_default_compile_commands_link "$canonical_repo")}"

  ge_dev_info "Development model: Fedora host-native"
  ge_dev_info "Canonical repository: $canonical_repo"
  ge_dev_info "Repo-local vcpkg: $vcpkg_root"
  ge_dev_info "clangd compile database link: $compile_commands_link"

  ge_dev_preflight "$canonical_repo" || status=1

  if [[ "$mode" == "check" ]]; then
    ge_dev_bootstrap_vcpkg "$canonical_repo" "$vcpkg_root" "$mode" || status=1
    ge_dev_install_pre_commit_hook "$canonical_repo" "$mode" || status=1
    ge_dev_check_compile_commands_link "$canonical_repo" "$compile_commands_link" || status=1
    ge_dev_detect_stale_compiler_caches "$canonical_repo" || true
    return "$status"
  fi

  if [[ "$status" -ne 0 ]]; then
    ge_dev_info "FAIL bootstrap.preflight prerequisites failed; generated state was not modified"
    return "$status"
  fi

  ge_dev_bootstrap_vcpkg "$canonical_repo" "$vcpkg_root" "$mode" || status=1
  ge_dev_install_pre_commit_hook "$canonical_repo" "$mode" || status=1
  ge_dev_create_compile_commands_link "$canonical_repo" "$compile_commands_link" || status=1
  ge_dev_detect_stale_compiler_caches "$canonical_repo" || true
  return "$status"
}
