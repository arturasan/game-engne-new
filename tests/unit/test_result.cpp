#include <doctest/doctest.h>

#include <string>

#include "engine/core/result.hpp"

namespace {

engine::Result<int> succeeds() {
    return 42;
}

engine::Result<int> fails() {
    return engine::Err{engine::ErrorCode::NotFound, "missing value"};
}

} // namespace

TEST_CASE("Result carries success values" * doctest::test_suite("fast")) {
    const engine::Result<int> result = succeeds();

    REQUIRE(result.has_value());
    CHECK(*result == 42);
}

TEST_CASE("Result carries Error failures" * doctest::test_suite("fast")) {
    const engine::Result<int> result = fails();

    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == engine::ErrorCode::NotFound);
    CHECK(result.error().message == "missing value");
    CHECK(std::string{result.error().where.function_name()}.find("fails") != std::string::npos);
}

TEST_CASE("Result<void> accepts Err helper" * doctest::test_suite("fast")) {
    const engine::Result<void> result =
        engine::Err{engine::ErrorCode::InvalidArgument, "bad argument"};

    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == engine::ErrorCode::InvalidArgument);
}
