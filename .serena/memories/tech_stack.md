# Tech Stack

- Language baseline: C++26, no compiler extensions. C++23 fallback is acceptable/expected where GCC 16 and Clang 20 differ on a C++26 feature.
- Supported platform through M4: Fedora 40+ only. Windows/macOS presets/docs remain in-tree but are unmaintained until M5.
- Primary toolchain: GCC 16, secondary Clang 20. Use Fedora Rawhide toolbox if host compiler is older.
- Build system: CMake 3.30+, Ninja Multi-Config for Linux presets, `mold` linker, `sccache` compiler launcher, vcpkg manifest mode.
- Active CMake presets: `linux-clang-asan` Debug with ASan/UBSan, `linux-gcc-rel` Release. `check` workflow configures/builds/tests `linux-clang-asan`.
- vcpkg baseline is pinned in `vcpkg.json`; current deps: `doctest`, `spdlog`, `glm`, `sdl3`. Existing architecture also plans miniaudio/stb/tinygltf/physics deps behind engine interfaces when their specs land.
- Current CMake target: `engine::engine` static/library target plus `engine_tests` and `examples/hello_window`.
- Tests: doctest. Fast tests are labels/test suites consumed by CTest presets; `cmake --workflow --preset check` runs `[fast]` only.
- Logging: public API in `engine/core/log.hpp`, spdlog hidden in `engine/core/spdlog_backend.cpp`; structured JSON sink writes `build/logs/last_run.jsonl`.
- Platform: public API in `engine/platform/*.hpp`, SDL3 hidden behind detail/backend implementation files.
- Rendering decision: ADR 0003 chooses SDL3 GPU backend through M1-M4; SDL3 GPU must remain hidden behind `engine/render` abstractions when implemented.
- ECS component IDs: ADR 0004 uses process-local monotonic `std::uint32_t` IDs, not serialized/stable schema identifiers.
- clangd: after first successful workflow, root `compile_commands.json` should symlink to `build/linux-clang-asan/compile_commands.json`.
