#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "engine/core/log.hpp"
#include "engine/platform/detail/window_backend.hpp"
#include "engine/platform/window.hpp"
#include "engine/render/detail/renderer_backend.hpp"

namespace engine::detail {

namespace {

[[nodiscard]] std::string sdl_error_message(std::string_view context) {
    std::string message{context};
    message += ": ";
    const char* error = SDL_GetError();
    message += error != nullptr && error[0] != '\0' ? error : "unknown SDL error";
    return message;
}

[[nodiscard]] Error backend_error(std::string_view context) {
    auto message = sdl_error_message(context);
    log::error("{}", message);
    return Error{ErrorCode::BackendError, std::move(message)};
}

[[nodiscard]] Error unsupported(std::string message) {
    log::error("{}", message);
    return Error{ErrorCode::UnsupportedFormat, std::move(message)};
}

[[nodiscard]] Color clamp_color(Color color) noexcept {
    return Color{
        .r = std::clamp(color.r, 0.0F, 1.0F),
        .g = std::clamp(color.g, 0.0F, 1.0F),
        .b = std::clamp(color.b, 0.0F, 1.0F),
        .a = std::clamp(color.a, 0.0F, 1.0F),
    };
}

[[nodiscard]] SDL_FColor to_backend_color(Color color) noexcept {
    const Color clamped = clamp_color(color);
    return SDL_FColor{.r = clamped.r, .g = clamped.g, .b = clamped.b, .a = clamped.a};
}

[[nodiscard]] const char* requested_gpu_driver() noexcept {
    const char* driver = std::getenv("SDL_GPU_DRIVER");
    if (driver == nullptr || driver[0] == '\0') {
        return nullptr;
    }
    return driver;
}

[[nodiscard]] Result<SDL_GPUDevice*> create_device(RendererConfig config) {
    SDL_GPUDevice* device =
        SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, config.debug, requested_gpu_driver());
    if (device == nullptr) {
        return std::unexpected(backend_error("SDL_CreateGPUDevice failed"));
    }
    return device;
}

[[nodiscard]] Result<void> submit_command_buffer(SDL_GPUCommandBuffer*& command_buffer) {
    assert(command_buffer != nullptr);
    if (!SDL_SubmitGPUCommandBuffer(command_buffer)) {
        command_buffer = nullptr;
        return std::unexpected(backend_error("SDL_SubmitGPUCommandBuffer failed"));
    }
    command_buffer = nullptr;
    return {};
}

[[nodiscard]] Result<void> cancel_command_buffer(SDL_GPUCommandBuffer*& command_buffer,
                                                 std::string_view context) {
    assert(command_buffer != nullptr);
    if (!SDL_CancelGPUCommandBuffer(command_buffer)) {
        command_buffer = nullptr;
        auto message = sdl_error_message(context);
        log::error("{}", message);
        return std::unexpected(Error{ErrorCode::BackendError, std::move(message)});
    }
    command_buffer = nullptr;
    return {};
}

void record_clear_pass(SDL_GPUCommandBuffer* command_buffer, SDL_GPUTexture* target,
                       Color clear_color) {
    SDL_GPUColorTargetInfo target_info{
        .texture = target,
        .mip_level = 0,
        .layer_or_depth_plane = 0,
        .clear_color = to_backend_color(clear_color),
        .load_op = SDL_GPU_LOADOP_CLEAR,
        .store_op = SDL_GPU_STOREOP_STORE,
        .resolve_texture = nullptr,
        .resolve_mip_level = 0,
        .resolve_layer = 0,
        .cycle = false,
        .cycle_resolve_texture = false,
        .padding1 = 0,
        .padding2 = 0,
    };
    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(command_buffer, &target_info, 1, nullptr);
    SDL_EndGPURenderPass(pass);
}

class SdlGpuRenderer final : public RendererBackend {
public:
    static Result<std::unique_ptr<RendererBackend>> create_for_window(Window& window,
                                                                      RendererConfig config) {
        NativeWindowLease lease = WindowAccess::acquire_native_lease(window);
        if (!lease) {
            return Err{ErrorCode::BackendError, "windowed renderer requires a native window lease"};
        }

        auto device_result = create_device(config);
        if (!device_result.has_value()) {
            return std::unexpected(device_result.error());
        }

        auto renderer = std::unique_ptr<SdlGpuRenderer>(new SdlGpuRenderer());
        renderer->window_lease_ = std::move(lease);
        renderer->window_ = static_cast<SDL_Window*>(renderer->window_lease_.native_handle());
        renderer->device_ = *device_result;
        renderer->size_ = window.size();
#ifndef NDEBUG
        renderer->owner_thread_ = renderer->window_lease_.creation_thread();
        assert(renderer->owner_thread_ == std::this_thread::get_id());
#endif

        if (!SDL_ClaimWindowForGPUDevice(renderer->device_, renderer->window_)) {
            return std::unexpected(backend_error("SDL_ClaimWindowForGPUDevice failed"));
        }
        renderer->window_claimed_ = true;

        auto present_result = renderer->configure_present_mode();
        if (!present_result.has_value()) {
            return std::unexpected(present_result.error());
        }

        return std::unique_ptr<RendererBackend>{std::move(renderer)};
    }

    static Result<std::unique_ptr<RendererBackend>>
    create_headless(HeadlessRenderConfig config, RendererConfig renderer_config) {
        if (config.size.width == 0 || config.size.height == 0) {
            return Err{ErrorCode::InvalidArgument, "headless renderer dimensions must be non-zero"};
        }

        auto video = PlatformSubsystemLease::acquire_headless_video();
        if (!video.has_value()) {
            return std::unexpected(video.error());
        }

        auto device_result = create_device(renderer_config);
        if (!device_result.has_value()) {
            return std::unexpected(device_result.error());
        }

        auto renderer = std::unique_ptr<SdlGpuRenderer>(new SdlGpuRenderer());
        renderer->video_lease_ = std::move(*video);
        renderer->device_ = *device_result;
        renderer->headless_ = true;
        renderer->size_ = config.size;
#ifndef NDEBUG
        renderer->owner_thread_ = std::this_thread::get_id();
#endif

        auto target = renderer->create_headless_target_texture(config.size);
        if (!target.has_value()) {
            return std::unexpected(target.error());
        }
        renderer->target_ = *target;
        return std::unique_ptr<RendererBackend>{std::move(renderer)};
    }

    ~SdlGpuRenderer() override {
        assert_thread();
        if (target_ != nullptr) {
            SDL_ReleaseGPUTexture(device_, target_);
            target_ = nullptr;
        }
        if (window_claimed_) {
            SDL_ReleaseWindowFromGPUDevice(device_, window_);
            window_claimed_ = false;
        }
        if (device_ != nullptr) {
            SDL_DestroyGPUDevice(device_);
            device_ = nullptr;
        }
        window_ = nullptr;
    }

    [[nodiscard]] Result<FrameStatus> render_clear(Color color) override {
        assert_thread();
        if (headless_) {
            return render_headless_clear(color);
        }
        return render_window_clear(color);
    }

    [[nodiscard]] Result<void> resize(Extent2d size) override {
        assert_thread();
        if (!headless_) {
            size_ = size;
            return {};
        }
        if (size.width == 0 || size.height == 0) {
            return Err{ErrorCode::InvalidArgument,
                       "headless renderer resize dimensions must be non-zero"};
        }
        auto new_target = create_headless_target_texture(size);
        if (!new_target.has_value()) {
            return std::unexpected(new_target.error());
        }

        SDL_GPUTexture* old_target = std::exchange(target_, *new_target);
        size_ = size;
        submitted_clear_ = false;
        if (old_target != nullptr) {
            SDL_ReleaseGPUTexture(device_, old_target);
        }
        return {};
    }

    [[nodiscard]] Result<ReadbackImage> read_back() override {
        assert_thread();
        if (!headless_) {
            return Err{ErrorCode::InvalidArgument,
                       "readback is only supported for headless renderers"};
        }
        if (!submitted_clear_) {
            return Err{ErrorCode::InvalidArgument,
                       "readback requires a successful submitted clear first"};
        }

        const std::uint32_t bytes_per_pixel = 4;
        const std::uint32_t bytes_per_row = size_.width * bytes_per_pixel;
        const std::uint32_t byte_count = bytes_per_row * size_.height;

        SDL_GPUTransferBufferCreateInfo transfer_info{
            .usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,
            .size = byte_count,
            .props = 0,
        };
        SDL_GPUTransferBuffer* transfer = SDL_CreateGPUTransferBuffer(device_, &transfer_info);
        if (transfer == nullptr) {
            return std::unexpected(backend_error("SDL_CreateGPUTransferBuffer failed"));
        }

        struct TransferGuard {
            SDL_GPUDevice* device = nullptr;
            SDL_GPUTransferBuffer* transfer = nullptr;
            ~TransferGuard() {
                if (transfer != nullptr) {
                    SDL_ReleaseGPUTransferBuffer(device, transfer);
                }
            }
        } transfer_guard{device_, transfer};

        SDL_GPUCommandBuffer* command_buffer = SDL_AcquireGPUCommandBuffer(device_);
        if (command_buffer == nullptr) {
            return std::unexpected(backend_error("SDL_AcquireGPUCommandBuffer failed"));
        }

        SDL_GPUCopyPass* copy_pass = SDL_BeginGPUCopyPass(command_buffer);
        SDL_GPUTextureRegion source{
            .texture = target_,
            .mip_level = 0,
            .layer = 0,
            .x = 0,
            .y = 0,
            .z = 0,
            .w = size_.width,
            .h = size_.height,
            .d = 1,
        };
        SDL_GPUTextureTransferInfo destination{
            .transfer_buffer = transfer,
            .offset = 0,
            .pixels_per_row = size_.width,
            .rows_per_layer = size_.height,
        };
        SDL_DownloadFromGPUTexture(copy_pass, &source, &destination);
        SDL_EndGPUCopyPass(copy_pass);

        SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(command_buffer);
        command_buffer = nullptr;
        if (fence == nullptr) {
            return std::unexpected(
                backend_error("SDL_SubmitGPUCommandBufferAndAcquireFence failed"));
        }

        struct FenceGuard {
            SDL_GPUDevice* device = nullptr;
            SDL_GPUFence* fence = nullptr;
            ~FenceGuard() {
                if (fence != nullptr) {
                    SDL_ReleaseGPUFence(device, fence);
                }
            }
        } fence_guard{device_, fence};

        SDL_GPUFence* fences[] = {fence};
        if (!SDL_WaitForGPUFences(device_, true, fences, 1)) {
            return std::unexpected(backend_error("SDL_WaitForGPUFences failed"));
        }

        void* mapped = SDL_MapGPUTransferBuffer(device_, transfer, false);
        if (mapped == nullptr) {
            return std::unexpected(backend_error("SDL_MapGPUTransferBuffer failed"));
        }

        struct MapGuard {
            SDL_GPUDevice* device = nullptr;
            SDL_GPUTransferBuffer* transfer = nullptr;
            ~MapGuard() {
                if (transfer != nullptr) {
                    SDL_UnmapGPUTransferBuffer(device, transfer);
                }
            }
        } map_guard{device_, transfer};

        ReadbackImage image{
            .size = size_,
            .bytes_per_row = bytes_per_row,
            .pixels = std::vector<std::byte>(byte_count),
        };
        std::memcpy(image.pixels.data(), mapped, image.pixels.size());
        return image;
    }

    [[nodiscard]] bool headless() const noexcept override {
        assert_thread();
        return headless_;
    }

    [[nodiscard]] Extent2d size() const noexcept override {
        assert_thread();
        return size_;
    }

private:
    SdlGpuRenderer() = default;

    void assert_thread() const noexcept {
#ifndef NDEBUG
        assert(owner_thread_ == std::this_thread::get_id());
#endif
    }

    [[nodiscard]] Result<void> configure_present_mode() {
        SDL_GPUSwapchainComposition composition = SDL_GPU_SWAPCHAINCOMPOSITION_SDR;
        if (!SDL_WindowSupportsGPUSwapchainComposition(device_, window_, composition)) {
            if (SDL_WindowSupportsGPUSwapchainComposition(
                    device_, window_, SDL_GPU_SWAPCHAINCOMPOSITION_SDR_LINEAR)) {
                composition = SDL_GPU_SWAPCHAINCOMPOSITION_SDR_LINEAR;
                log::warn("SDR swapchain composition unavailable; falling back to SDR_LINEAR");
            } else {
                const std::string message = "no supported SDR swapchain composition";
                log::error("{}", message);
                return std::unexpected(Error{ErrorCode::BackendError, message});
            }
        }

        SDL_GPUPresentMode present_mode = SDL_GPU_PRESENTMODE_VSYNC;
        if (!window_lease_.vsync_requested()) {
            if (SDL_WindowSupportsGPUPresentMode(device_, window_, SDL_GPU_PRESENTMODE_IMMEDIATE)) {
                present_mode = SDL_GPU_PRESENTMODE_IMMEDIATE;
            } else if (SDL_WindowSupportsGPUPresentMode(device_, window_,
                                                        SDL_GPU_PRESENTMODE_MAILBOX)) {
                present_mode = SDL_GPU_PRESENTMODE_MAILBOX;
            } else {
                log::warn("non-vsync present mode unavailable; falling back to vsync");
            }
        }

        if (!SDL_SetGPUSwapchainParameters(device_, window_, composition, present_mode)) {
            return std::unexpected(backend_error("SDL_SetGPUSwapchainParameters failed"));
        }
        return {};
    }

    [[nodiscard]] Result<SDL_GPUTexture*> create_headless_target_texture(Extent2d size) {
        constexpr auto format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        constexpr SDL_GPUTextureUsageFlags usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
        if (!SDL_GPUTextureSupportsFormat(device_, format, SDL_GPU_TEXTURETYPE_2D, usage)) {
            return std::unexpected(
                unsupported("RGBA8_UNORM color target is not supported by SDL GPU backend"));
        }

        SDL_GPUTextureCreateInfo info{
            .type = SDL_GPU_TEXTURETYPE_2D,
            .format = format,
            .usage = usage,
            .width = size.width,
            .height = size.height,
            .layer_count_or_depth = 1,
            .num_levels = 1,
            .sample_count = SDL_GPU_SAMPLECOUNT_1,
            .props = 0,
        };
        SDL_GPUTexture* texture = SDL_CreateGPUTexture(device_, &info);
        if (texture == nullptr) {
            return std::unexpected(backend_error("SDL_CreateGPUTexture failed"));
        }
        return texture;
    }

    [[nodiscard]] Result<FrameStatus> render_headless_clear(Color color) {
        SDL_GPUCommandBuffer* command_buffer = SDL_AcquireGPUCommandBuffer(device_);
        if (command_buffer == nullptr) {
            return std::unexpected(backend_error("SDL_AcquireGPUCommandBuffer failed"));
        }
        record_clear_pass(command_buffer, target_, color);
        auto submit = submit_command_buffer(command_buffer);
        if (!submit.has_value()) {
            return std::unexpected(submit.error());
        }
        submitted_clear_ = true;
        return FrameStatus::Rendered;
    }

    [[nodiscard]] Result<FrameStatus> render_window_clear(Color color) {
        if (size_.width == 0 || size_.height == 0) {
            return FrameStatus::Skipped;
        }

        SDL_GPUCommandBuffer* command_buffer = SDL_AcquireGPUCommandBuffer(device_);
        if (command_buffer == nullptr) {
            return std::unexpected(backend_error("SDL_AcquireGPUCommandBuffer failed"));
        }

        SDL_GPUTexture* swapchain_texture = nullptr;
        Uint32 swapchain_width = 0;
        Uint32 swapchain_height = 0;
        if (!SDL_WaitAndAcquireGPUSwapchainTexture(command_buffer, window_, &swapchain_texture,
                                                   &swapchain_width, &swapchain_height)) {
            auto acquisition_error =
                sdl_error_message("SDL_WaitAndAcquireGPUSwapchainTexture failed");
            log::error("{}", acquisition_error);
            auto cancel =
                cancel_command_buffer(command_buffer, "SDL_CancelGPUCommandBuffer failed after "
                                                      "swapchain acquisition failure");
            if (!cancel.has_value()) {
                acquisition_error += "; ";
                acquisition_error += cancel.error().message;
            }
            return std::unexpected(Error{ErrorCode::BackendError, std::move(acquisition_error)});
        }

        if (swapchain_texture == nullptr) {
            auto cancel = cancel_command_buffer(command_buffer,
                                                "SDL_CancelGPUCommandBuffer failed after null "
                                                "swapchain texture");
            if (!cancel.has_value()) {
                return std::unexpected(cancel.error());
            }
            return FrameStatus::Skipped;
        }

        size_ = Extent2d{.width = swapchain_width, .height = swapchain_height};
        record_clear_pass(command_buffer, swapchain_texture, color);
        auto submit = submit_command_buffer(command_buffer);
        if (!submit.has_value()) {
            return std::unexpected(submit.error());
        }
        return FrameStatus::Rendered;
    }

    PlatformSubsystemLease video_lease_;
    NativeWindowLease window_lease_;
    SDL_GPUDevice* device_ = nullptr;
    SDL_Window* window_ = nullptr;
    SDL_GPUTexture* target_ = nullptr;
    Extent2d size_{};
    bool headless_ = false;
    bool window_claimed_ = false;
    bool submitted_clear_ = false;
#ifndef NDEBUG
    std::thread::id owner_thread_{};
#endif
};

} // namespace

Result<std::unique_ptr<RendererBackend>> create_window_renderer_backend(Window& window,
                                                                        RendererConfig config) {
    return SdlGpuRenderer::create_for_window(window, config);
}

Result<std::unique_ptr<RendererBackend>>
create_headless_renderer_backend(HeadlessRenderConfig config, RendererConfig renderer_config) {
    return SdlGpuRenderer::create_headless(config, renderer_config);
}

} // namespace engine::detail
