#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "engine/core/app.hpp"
#include "engine/core/result.hpp"
#include "engine/platform/types.hpp"
#include "engine/platform/window.hpp"

namespace engine {

struct Color {
    float r = 0.0F;
    float g = 0.0F;
    float b = 0.0F;
    float a = 1.0F;
};

struct ClearColor {
    Color value{0.1F, 0.1F, 0.1F, 1.0F};
};

struct RendererConfig {
    bool debug = false;
};

struct HeadlessRenderConfig {
    Extent2d size{256, 256};
};

struct ReadbackImage {
    Extent2d size{};
    std::uint32_t bytes_per_row = 0;
    std::vector<std::byte> pixels;
};

enum class FrameStatus {
    Rendered,
    Skipped,
};

class Renderer {
public:
    static Result<Renderer> create_for_window(Window& window, RendererConfig config = {});
    static Result<Renderer> create_headless(HeadlessRenderConfig config = {},
                                            RendererConfig renderer_config = {});

    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&) noexcept;
    Renderer& operator=(Renderer&&) noexcept;

    [[nodiscard]] Result<FrameStatus> render_clear(Color color);
    [[nodiscard]] Result<void> resize(Extent2d size);
    [[nodiscard]] Result<ReadbackImage> read_back();

    [[nodiscard]] bool headless() const noexcept;
    [[nodiscard]] Extent2d size() const noexcept;

private:
    class Impl;

    explicit Renderer(std::unique_ptr<Impl> impl) noexcept;

    std::unique_ptr<Impl> impl_;
};

struct RenderPlugin {
    RendererConfig config{};
    HeadlessRenderConfig headless{};
    void build(App& app) const;
};

} // namespace engine
