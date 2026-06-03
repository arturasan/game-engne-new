#include <doctest/doctest.h>

#include <cstdint>

#include "engine/ecs/world.hpp"

namespace {

struct Position {
    int x = 0;
    int y = 0;
};

struct Velocity {
    int dx = 0;
    int dy = 0;
};

struct Health {
    int hp = 0;
};

struct Tag {};

} // namespace

static_assert(engine::detail::unique_component_types<>);
static_assert(engine::detail::unique_component_types<Position>);
static_assert(engine::detail::unique_component_types<Position, Velocity, const Health&>);
static_assert(!engine::detail::unique_component_types<Position, Position>);
static_assert(!engine::detail::unique_component_types<Position, const Position>);
static_assert(!engine::detail::unique_component_types<Position&, const Position&>);

TEST_CASE("World::spawn creates live entities" * doctest::test_suite("fast")) {
    engine::World world;

    const engine::Entity entity = world.spawn();

    CHECK(world.alive(entity));
}

TEST_CASE("World::despawn invalidates an entity" * doctest::test_suite("fast")) {
    engine::World world;
    const engine::Entity entity = world.spawn();

    CHECK(world.despawn(entity));

    CHECK_FALSE(world.alive(entity));
    CHECK_FALSE(world.despawn(entity));
}

TEST_CASE("World reuses freed slots with a bumped generation" * doctest::test_suite("fast")) {
    engine::World world;
    const engine::Entity stale = world.spawn();
    CHECK(world.despawn(stale));

    const engine::Entity reused = world.spawn();

    CHECK(reused.id == stale.id);
    CHECK(reused.generation == stale.generation + 1U);
    CHECK_FALSE(world.alive(stale));
    CHECK(world.alive(reused));
}

TEST_CASE("Stale entity handles fail component operations cleanly" * doctest::test_suite("fast")) {
    engine::World world;
    const engine::Entity stale = world.spawn();
    CHECK(world.add<Position>(stale, Position{1, 2}) != nullptr);
    CHECK(world.despawn(stale));

    CHECK_FALSE(world.alive(stale));
    CHECK(world.get<Position>(stale) == nullptr);
    CHECK_FALSE(world.has<Position>(stale));
    CHECK(world.add<Velocity>(stale, Velocity{3, 4}) == nullptr);
    CHECK_FALSE(world.remove<Position>(stale));
}

TEST_CASE("World supports add get and has for POD components" * doctest::test_suite("fast")) {
    engine::World world;
    const engine::Entity entity = world.spawn();

    Position* position = world.add<Position>(entity, Position{10, 20});

    REQUIRE(position != nullptr);
    CHECK(position->x == 10);
    CHECK(position->y == 20);
    CHECK(world.has<Position>(entity));
    REQUIRE(world.get<Position>(entity) != nullptr);
    CHECK(world.get<Position>(entity)->x == 10);
}

TEST_CASE("World removes components and can add them again" * doctest::test_suite("fast")) {
    engine::World world;
    const engine::Entity entity = world.spawn();
    CHECK(world.add<Position>(entity, Position{1, 2}) != nullptr);

    CHECK(world.remove<Position>(entity));
    CHECK_FALSE(world.has<Position>(entity));
    CHECK(world.get<Position>(entity) == nullptr);
    CHECK_FALSE(world.remove<Position>(entity));

    CHECK(world.add<Position>(entity, Position{3, 4}) != nullptr);
    REQUIRE(world.get<Position>(entity) != nullptr);
    CHECK(world.get<Position>(entity)->x == 3);
}

TEST_CASE("World moves entities between several archetypes" * doctest::test_suite("fast")) {
    engine::World world;
    const engine::Entity entity = world.spawn();

    CHECK(world.add<Position>(entity, Position{1, 2}) != nullptr);
    CHECK(world.add<Velocity>(entity, Velocity{3, 4}) != nullptr);
    CHECK(world.add<Health>(entity, Health{5}) != nullptr);
    CHECK(world.has<Position>(entity));
    CHECK(world.has<Velocity>(entity));
    CHECK(world.has<Health>(entity));

    CHECK(world.remove<Velocity>(entity));
    CHECK(world.has<Position>(entity));
    CHECK_FALSE(world.has<Velocity>(entity));
    CHECK(world.has<Health>(entity));

    CHECK(world.add<Tag>(entity, Tag{}) != nullptr);
    CHECK(world.has<Tag>(entity));
    REQUIRE(world.get<Health>(entity) != nullptr);
    CHECK(world.get<Health>(entity)->hp == 5);
}

TEST_CASE("World does not repair entity zero row when erasing an unswapped last row" *
          doctest::test_suite("fast")) {
    SUBCASE("remove component from another last-row entity") {
        engine::World world;
        const engine::Entity zero = world.spawn();
        const engine::Entity filler = world.spawn(Position{1, 2});
        CHECK(world.add<Position>(zero, Position{10, 20}) != nullptr);
        const engine::Entity remove_last = world.spawn(Position{30, 40}, Velocity{1, 2});

        CHECK(zero.id == 0U);
        REQUIRE(world.get<Position>(zero) != nullptr);
        CHECK(world.get<Position>(zero)->x == 10);
        CHECK(world.get<Position>(filler)->x == 1);

        CHECK(world.remove<Velocity>(remove_last));

        REQUIRE(world.get<Position>(zero) != nullptr);
        CHECK(world.get<Position>(zero)->x == 10);
        CHECK(world.get<Position>(zero)->y == 20);
    }

    SUBCASE("despawn another last-row entity") {
        engine::World world;
        const engine::Entity zero = world.spawn();
        const engine::Entity filler = world.spawn(Position{1, 2});
        CHECK(world.add<Position>(zero, Position{10, 20}) != nullptr);
        const engine::Entity despawn_last = world.spawn(Position{50, 60}, Health{70});

        CHECK(zero.id == 0U);
        REQUIRE(world.get<Position>(zero) != nullptr);
        CHECK(world.get<Position>(zero)->x == 10);
        CHECK(world.get<Position>(filler)->x == 1);

        CHECK(world.despawn(despawn_last));

        REQUIRE(world.get<Position>(zero) != nullptr);
        CHECK(world.get<Position>(zero)->x == 10);
        CHECK(world.get<Position>(zero)->y == 20);
    }
}

TEST_CASE("World iterates entities with two components and skips partial matches" *
          doctest::test_suite("fast")) {
    engine::World world;
    const engine::Entity both = world.spawn(Position{1, 2}, Velocity{3, 4});
    const engine::Entity only_position = world.spawn(Position{10, 20});
    const engine::Entity only_velocity = world.spawn(Velocity{30, 40});

    int visited = 0;
    world.for_each<Position, Velocity>(
        [&](engine::Entity entity, Position& position, Velocity& velocity) {
            CHECK(entity == both);
            CHECK(entity != only_position);
            CHECK(entity != only_velocity);
            CHECK(position.x == 1);
            CHECK(velocity.dx == 3);
            ++visited;
        });

    CHECK(visited == 1);
}

TEST_CASE("World iterates 100k entities across multiple archetypes" * doctest::test_suite("slow")) {
    engine::World world;
    constexpr std::uint32_t count = 100'000;
    std::uint64_t expected_sum = 0;

    for (std::uint32_t i = 0; i < count; ++i) {
        if (i % 2U == 0U) {
            world.spawn(Position{static_cast<int>(i), 1}, Velocity{2, 3});
            expected_sum += i;
        } else if (i % 3U == 0U) {
            world.spawn(Position{static_cast<int>(i), 1}, Health{7});
        } else {
            world.spawn(Velocity{2, 3}, Health{9});
        }
    }

    std::uint32_t visited = 0;
    std::uint64_t sum = 0;
    world.for_each<Position, Velocity>(
        [&](engine::Entity, const Position& position, const Velocity&) {
            ++visited;
            sum += static_cast<std::uint32_t>(position.x);
        });

    CHECK(visited == count / 2U);
    CHECK(sum == expected_sum);
}
