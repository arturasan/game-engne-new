# Suggested Commands

- First-time deps: `git submodule update --init --recursive`.
- Fast inner loop: `cmake --workflow --preset check`.
- Configure/build/test Clang ASan preset: `cmake --preset linux-clang-asan && cmake --build --preset linux-clang-asan && ctest --preset linux-clang-asan`.
- GCC release path: `cmake --preset linux-gcc-rel && cmake --build --preset linux-gcc-rel && ctest --preset linux-gcc-rel`.
- Run current example: `./build/linux-clang-asan/Debug/examples/hello_window/hello_window`.
- Public-header dependency canary: `grep -rE 'SDL_|SDL_GPU|glm::|spdlog|ImGui' engine/**/*.hpp`.
- Inspect latest example logs before debugging runtime behavior: `grep -n . build/logs/last_run.jsonl`.
- clangd compile DB after workflow: `ln -sfn build/linux-clang-asan/compile_commands.json compile_commands.json`.
- Fedora toolbox path for GCC 16/Clang 20 if host is older: `toolbox create --image registry.fedoraproject.org/fedora:rawhide engine` then `toolbox enter engine`.
- Headless screenshot parity later: set llvmpipe via `VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json`; do not update `tests/refs` PNGs by hand.
- Memory sanity check after onboarding/maintenance: `serena memories check` from project root.
