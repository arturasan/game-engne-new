#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "engine/core/app.hpp"

TEST_CASE("App::run exits cleanly when max_frames is set" * doctest::test_suite("fast")) {
    engine::App app;
    app.set_max_frames(3);
    CHECK(app.run() == 0);
    CHECK(app.frame() == 3);
}

TEST_CASE("App::run honors request_exit from a system" * doctest::test_suite("fast")) {
    engine::App app;
    app.add_system("quit", [](engine::App& a) {
        if (a.frame() == 5) {
            a.request_exit();
        }
    });
    CHECK(app.run() == 0);
    CHECK(app.frame() == 6);
}

TEST_CASE("Plugin::build is invoked by add_plugin" * doctest::test_suite("fast")) {
    struct CountPlugin {
        int* counter;
        void build(engine::App&) const {
            ++*counter;
        }
    };
    int counter = 0;
    engine::App app;
    app.add_plugin(CountPlugin{&counter}).set_max_frames(1).run();
    CHECK(counter == 1);
}
