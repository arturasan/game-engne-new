#include "engine/render/renderer.hpp"

#include <cassert>
#include <cstdlib>
#include <string>

#include "engine/core/log.hpp"
#include "engine/ecs/events.hpp"
#include "engine/platform/events.hpp"
#include "engine/render/detail/renderer_backend.hpp"

namespace engine {

namespace {

[[nodiscard]] bool headless_requested() noexcept {
    const char* value = std::getenv("ENGINE_HEADLESS");
    return value != nullptr && value[0] != '\0' && std::string_view(value) != "0";
}

void render_clear_system(App& app, Res<ClearColor> clear_color, ResMut<Renderer> renderer,
                         EventReader<WindowResizeEvent> resize_events) {
    Extent2d final_size = renderer->size();
    bool resized = false;
    for (const auto& event : resize_events.read()) {
        final_size = event.size;
        resized = true;
    }

    if (resized) {
        auto resize_result = renderer->resize(final_size);
        if (!resize_result.has_value()) {
            log::error("renderer resize failed: {}", resize_result.error().message);
            app.request_exit();
            return;
        }
    }

    auto frame = renderer->render_clear(clear_color->value);
    if (!frame.has_value()) {
        log::error("renderer clear failed: {}", frame.error().message);
        app.request_exit();
        return;
    }
}

} // namespace

class Renderer::Impl {
public:
    explicit Impl(std::unique_ptr<detail::RendererBackend> backend) noexcept
        : backend_(std::move(backend)) {}

    [[nodiscard]] detail::RendererBackend& backend() noexcept {
        assert(backend_ != nullptr);
        return *backend_;
    }

    [[nodiscard]] const detail::RendererBackend& backend() const noexcept {
        assert(backend_ != nullptr);
        return *backend_;
    }

private:
    std::unique_ptr<detail::RendererBackend> backend_;
};

Renderer::Renderer(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {}

Result<Renderer> Renderer::create_for_window(Window& window, RendererConfig config) {
    auto backend = detail::create_window_renderer_backend(window, config);
    if (!backend.has_value()) {
        return std::unexpected(backend.error());
    }
    return Renderer{std::make_unique<Impl>(std::move(*backend))};
}

Result<Renderer> Renderer::create_headless(HeadlessRenderConfig config,
                                           RendererConfig renderer_config) {
    auto backend = detail::create_headless_renderer_backend(config, renderer_config);
    if (!backend.has_value()) {
        return std::unexpected(backend.error());
    }
    return Renderer{std::make_unique<Impl>(std::move(*backend))};
}

Renderer::~Renderer() = default;
Renderer::Renderer(Renderer&&) noexcept = default;
Renderer& Renderer::operator=(Renderer&&) noexcept = default;

Result<FrameStatus> Renderer::render_clear(Color color) {
    if (impl_ == nullptr) {
        return Err{ErrorCode::InvalidArgument, "renderer is empty"};
    }
    return impl_->backend().render_clear(color);
}

Result<void> Renderer::resize(Extent2d size) {
    if (impl_ == nullptr) {
        return Err{ErrorCode::InvalidArgument, "renderer is empty"};
    }
    return impl_->backend().resize(size);
}

Result<ReadbackImage> Renderer::read_back() {
    if (impl_ == nullptr) {
        return Err{ErrorCode::InvalidArgument, "renderer is empty"};
    }
    return impl_->backend().read_back();
}

bool Renderer::headless() const noexcept {
    return impl_ != nullptr && impl_->backend().headless();
}

Extent2d Renderer::size() const noexcept {
    if (impl_ == nullptr) {
        return {};
    }
    return impl_->backend().size();
}

void RenderPlugin::build(App& app) const {
    if (app.world().try_resource<Renderer>() != nullptr) {
        log::error("RenderPlugin registered more than once");
        assert(false && "RenderPlugin registered more than once");
        return;
    }

    if (app.world().try_resource<ClearColor>() == nullptr) {
        app.world().insert_resource(ClearColor{});
    }
    if (app.world().try_resource<Events<WindowResizeEvent>>() == nullptr) {
        app.add_event<WindowResizeEvent>();
    }

    Result<Renderer> renderer_result = headless_requested()
                                           ? Renderer::create_headless(headless, config)
                                           : [&]() -> Result<Renderer> {
        auto* window = app.world().try_resource<Window>();
        if (window == nullptr) {
            return Err{ErrorCode::BackendError, "RenderPlugin requires a Window resource "
                                                "outside headless mode"};
        }
        return Renderer::create_for_window(*window, config);
    }();

    if (!renderer_result.has_value()) {
        log::error("renderer initialization failed: {}", renderer_result.error().message);
        app.request_exit();
        return;
    }

    app.world().insert_resource(std::move(*renderer_result));
    app.update().add("render_clear",
                     [](App& running_app, Res<ClearColor> clear_color, ResMut<Renderer> renderer,
                        EventReader<WindowResizeEvent> resize_events) {
                         render_clear_system(running_app, clear_color, renderer, resize_events);
                     });
}

} // namespace engine
