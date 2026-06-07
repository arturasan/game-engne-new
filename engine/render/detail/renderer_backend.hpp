#pragma once

#include <memory>

#include "engine/core/result.hpp"
#include "engine/platform/types.hpp"
#include "engine/platform/window.hpp"
#include "engine/render/renderer.hpp"

namespace engine::detail {

class RendererBackend {
public:
    virtual ~RendererBackend() = default;
    [[nodiscard]] virtual Result<FrameStatus> render_clear(Color color) = 0;
    [[nodiscard]] virtual Result<void> resize(Extent2d size) = 0;
    [[nodiscard]] virtual Result<ReadbackImage> read_back() = 0;
    [[nodiscard]] virtual bool headless() const noexcept = 0;
    [[nodiscard]] virtual Extent2d size() const noexcept = 0;
};

[[nodiscard]] Result<std::unique_ptr<RendererBackend>>
create_window_renderer_backend(Window& window, RendererConfig config);

[[nodiscard]] Result<std::unique_ptr<RendererBackend>>
create_headless_renderer_backend(HeadlessRenderConfig config, RendererConfig renderer_config);

} // namespace engine::detail
