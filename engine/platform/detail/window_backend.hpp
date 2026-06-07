#pragma once

#include <cstdint>
#include <memory>
#include <thread>
#include <vector>

#include "engine/core/result.hpp"
#include "engine/platform/events.hpp"
#include "engine/platform/window.hpp"

namespace engine::detail {

class PlatformSubsystemLease {
public:
    PlatformSubsystemLease() = default;
    ~PlatformSubsystemLease();

    PlatformSubsystemLease(const PlatformSubsystemLease&) = delete;
    PlatformSubsystemLease& operator=(const PlatformSubsystemLease&) = delete;
    PlatformSubsystemLease(PlatformSubsystemLease&& other) noexcept;
    PlatformSubsystemLease& operator=(PlatformSubsystemLease&& other) noexcept;

    [[nodiscard]] static Result<PlatformSubsystemLease> acquire_window();
    [[nodiscard]] static Result<PlatformSubsystemLease> acquire_headless_video();

private:
    explicit PlatformSubsystemLease(std::uint32_t subsystem_flags) noexcept
        : subsystem_flags_(subsystem_flags) {}

    void reset() noexcept;

    std::uint32_t subsystem_flags_ = 0;
};

class WindowBackend {
public:
    virtual ~WindowBackend() = default;
    virtual void poll_events(std::vector<PlatformEvent>& events) = 0;
    virtual void swap() = 0;
    [[nodiscard]] virtual void* native_handle() noexcept = 0;
    [[nodiscard]] virtual bool vsync_requested() const noexcept = 0;
    [[nodiscard]] virtual std::thread::id creation_thread() const noexcept = 0;
};

[[nodiscard]] std::shared_ptr<WindowBackend> create_window_backend(const WindowConfig& config);
[[nodiscard]] bool should_use_headless_backend() noexcept;

} // namespace engine::detail
