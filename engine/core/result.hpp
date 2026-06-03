#pragma once

#include <cstdint>
#include <expected>
#include <source_location>
#include <string>
#include <utility>

namespace engine {

enum class ErrorCode : std::uint32_t {
    Ok = 0,
    NotFound,
    PermissionDenied,
    InvalidArgument,
    OutOfMemory,
    UnsupportedFormat,
    BackendError,
    Cancelled,
    Timeout,
};

struct Error {
    ErrorCode code = ErrorCode::Ok;
    std::string message;
    std::source_location where = std::source_location::current();
};

template <typename T> using Result = std::expected<T, Error>;

struct Err {
    Error error;

    Err(ErrorCode code, std::string message,
        std::source_location where = std::source_location::current())
        : error{code, std::move(message), where} {}

    explicit Err(Error error_value) : error{std::move(error_value)} {}

    template <typename T> [[nodiscard]] operator Result<T>() const {
        return std::unexpected(error);
    }

    [[nodiscard]] operator std::unexpected<Error>() const {
        return std::unexpected(error);
    }
};

} // namespace engine
