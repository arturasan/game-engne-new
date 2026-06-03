#include <doctest/doctest.h>

#include <cstddef>

#include "engine/core/app.hpp"
#include "engine/ecs/events.hpp"

namespace {

struct Ping {
    int value = 0;
};

struct Pong {
    int value = 0;
};

struct Counter {
    int value = 0;
};

} // namespace

TEST_CASE("Events send and read current frame values" * doctest::test_suite("fast")) {
    engine::Events<Ping> events;

    events.send(Ping{4});

    const auto unread = events.read();
    REQUIRE(unread.size() == 1U);
    CHECK(unread[0].value == 4);
}

TEST_CASE("Events keep values for current and next frame then drop them" *
          doctest::test_suite("fast")) {
    engine::Events<Ping> events;
    events.send(Ping{5});

    CHECK(events.read().size() == 1U);
    events.update();
    CHECK(events.read().size() == 1U);
    events.update();
    CHECK(events.read().empty());
}

TEST_CASE("Events keep multiple channels isolated" * doctest::test_suite("fast")) {
    engine::Events<Ping> pings;
    engine::Events<Pong> pongs;

    pings.send(Ping{1});
    pongs.send(Pong{2});

    REQUIRE(pings.read().size() == 1U);
    REQUIRE(pongs.read().size() == 1U);
    CHECK(pings.read()[0].value == 1);
    CHECK(pongs.read()[0].value == 2);
}

TEST_CASE("Schedule still accepts App reference systems" * doctest::test_suite("fast")) {
    engine::App app;
    app.add_system("quit", [](engine::App& running_app) { running_app.request_exit(); });

    CHECK(app.run() == 0);
    CHECK(app.frame() == 1U);
}

TEST_CASE("Schedule injects immutable resources" * doctest::test_suite("fast")) {
    engine::App app;
    app.world().insert_resource(Counter{11});
    int observed = 0;
    app.add_system("read_counter",
                   [&observed](engine::Res<Counter> counter) { observed = counter->value; });

    app.set_max_frames(1).run();

    CHECK(observed == 11);
}

TEST_CASE("Schedule injects mutable resources" * doctest::test_suite("fast")) {
    engine::App app;
    app.world().insert_resource(Counter{1});
    app.add_system("mutate_counter", [](engine::ResMut<Counter> counter) { counter->value += 4; });

    app.set_max_frames(1).run();

    CHECK(app.world().resource<Counter>().value == 5);
}

TEST_CASE("EventWriter sends from a system" * doctest::test_suite("fast")) {
    engine::App app;
    app.add_event<Ping>();
    app.add_system("write_ping", [](engine::EventWriter<Ping> writer) { writer.send(Ping{8}); });

    app.set_max_frames(1).run();

    const auto events = app.world().resource<engine::Events<Ping>>().read();
    REQUIRE(events.size() == 1U);
    CHECK(events[0].value == 8);
}

TEST_CASE("EventReader sees each event once per reader" * doctest::test_suite("fast")) {
    engine::App app;
    app.add_event<Ping>();
    std::size_t seen = 0;
    app.add_system("write_ping_once",
                   [](engine::App& running_app, engine::EventWriter<Ping> writer) {
                       if (running_app.frame() == 0U) {
                           writer.send(Ping{1});
                       }
                   });
    app.add_system("read_ping",
                   [&seen](engine::EventReader<Ping> reader) { seen += reader.read().size(); });

    app.set_max_frames(2).run();

    CHECK(seen == 1U);
}

TEST_CASE("Multiple EventReaders keep independent cursors" * doctest::test_suite("fast")) {
    engine::App app;
    app.add_event<Ping>();
    std::size_t first_seen = 0;
    std::size_t second_seen = 0;
    app.add_system("write_ping_once",
                   [](engine::App& running_app, engine::EventWriter<Ping> writer) {
                       if (running_app.frame() == 0U) {
                           writer.send(Ping{3});
                       }
                   });
    app.add_system("first_reader", [&first_seen](engine::EventReader<Ping> reader) {
        first_seen += reader.read().size();
    });
    app.add_system("second_reader", [&second_seen](engine::EventReader<Ping> reader) {
        second_seen += reader.read().size();
    });

    app.set_max_frames(2).run();

    CHECK(first_seen == 1U);
    CHECK(second_seen == 1U);
}

TEST_CASE("Schedule records resource access metadata" * doctest::test_suite("fast")) {
    engine::App app;
    app.add_system("write_counter_a", [](engine::ResMut<Counter> counter) { ++counter->value; });
    app.add_system("write_counter_b", [](engine::ResMut<Counter> counter) { ++counter->value; });

    const auto systems = app.update().systems();

    REQUIRE(systems.size() == 2U);
    CHECK(systems[0].access.writes(engine::resource_id_for<Counter>()));
    CHECK(systems[1].access.writes(engine::resource_id_for<Counter>()));
    CHECK(systems[0].access.conflicts_with(systems[1].access));
}
