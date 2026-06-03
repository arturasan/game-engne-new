# Scheduler — concurrency model

## Scope

The engine's concurrency primitives, system parallelism, and async task handling. Everything that runs off the main thread goes through this layer.

## Backend

`stdexec` (the [P2300 reference implementation](https://github.com/NVIDIA/stdexec)) provides senders/receivers. Through M4 we target Fedora with GCC 16 + Clang 20 (ADR 0002), and we still standardize on stdexec so scheduler behavior stays consistent and decoupled from libc++/libstdc++ implementation differences. stdexec is the same API shape, just from a vendored repository.

The dependency is hidden behind `engine/scheduler/scheduler.hpp`. Plugin authors and game code call `engine::*` types and never see `stdexec::*`.

## Public API (target shape)

```cpp
namespace engine {

// ----- Pools -----
// The main pool: N-1 OS threads where N = hardware concurrency. The main thread
// participates as worker N when blocked on a sync point.
class TaskPool {
public:
    static TaskPool& main();           // CPU work, parallel system execution
    static TaskPool& io();             // blocking file/network I/O, smaller (4 threads default)

    template <typename F>
    auto submit(F&& f) -> Task<std::invoke_result_t<F>>;
};

// ----- Task<T> -----
// Owning handle to an in-flight async result.
template <typename T> class Task {
public:
    bool ready() const noexcept;
    T    wait();                       // blocks; participates in TaskPool::main
    template <typename Cont> void then(Cont&& c);
};

// ----- Parallel iteration (used by Query::par_each) -----
template <typename It, typename F>
void parallel_for(It begin, It end, F f, std::size_t grain = 0);

}  // namespace engine
```

## System parallelism

`Schedule::run()` builds a **conflict graph** from each system's declared access:

- A system writes `Transform`, reads `Velocity` → declared access `{W: Transform, R: Velocity}`.
- Two systems conflict if either writes a component the other reads or writes.
- Independent systems run in parallel on `TaskPool::main`.
- Systems are scheduled greedily: any system whose deps are satisfied and which conflicts with nothing currently running is dispatched.

The conflict graph is rebuilt only when systems are added/removed (rare); per-frame execution is cheap.

For M1, the executor is **single-threaded** (parallelism is wiring stub, scheduled to land in spec 0004 alongside the API). The conflict graph still gets built so we catch missing access declarations early.

## Coroutines

C++20 coroutines + stdexec senders. Asset loaders, network handlers, and animation streaming all live as coroutines:

```cpp
Task<Image> load_image_async(std::string_view path) {
    auto bytes = co_await read_file(path);          // suspends on TaskPool::io
    auto img   = co_await decode_png(bytes);        // suspends on TaskPool::main
    co_return img;
}
```

`Task<T>` is the awaiter type. `read_file` and `decode_png` are exposed by the engine as sender-returning functions.

## Determinism and parallelism

Parallel system execution **does not** affect determinism:

- The order of *writes* to a given component is deterministic because of the conflict graph (no two writers run simultaneously).
- Within a system, iteration order is deterministic (see `02-ecs.md`).
- Floating-point reproducibility across machines is a separate problem and is **not** guaranteed (see `07-testing.md`).

`parallel_for` with reduction operations needs an explicit ordered combine if order matters — provide an example in the doctest suite when M2 implements it.

## Thread affinity

- **Main thread:** owns the window event loop, GPU work submission, and the `App::run()` driver. Some platforms (macOS specifically) require windowing on the main thread.
- **TaskPool::main workers:** ECS systems, simulation, render-world extract.
- **TaskPool::io workers:** blocking file/network. Sized smaller because they spend time waiting, not computing.
- **Audio thread:** owned by miniaudio; we receive a callback. Never touched by app code directly.

## Decisions & alternatives

| Decision | Rationale | Rejected |
|---|---|---|
| stdexec reference impl, not stdlib `std::execution` | Uniform across all 3 compilers in 2026 | stdlib (uneven coverage causes silent perf cliffs) |
| Two pools (CPU, IO) | Different work characteristics; IO blocking shouldn't starve CPU | Single pool (causes IO blocking to murder frame time) |
| Single-threaded executor in M1, parallel in M2 | Get the API right first; parallelism is wiring | Big-bang parallel from day one (debugging hell for early specs) |
| Conflict graph built eagerly | Catches missing access declarations early | Lazy / on first violation (silent corruption) |

## Open questions

- Work-stealing vs explicit queue per worker — measure in M2.
- Whether to expose a `parallel_for_each` on `Query` directly — yes, but defer wiring to M2.
- Job priorities — needed for audio buffer fills? Probably not (audio is its own thread). Revisit if needed.
