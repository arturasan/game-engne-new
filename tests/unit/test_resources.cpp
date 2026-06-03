#include <doctest/doctest.h>

#include "engine/ecs/world.hpp"

namespace {

struct CounterResource {
    int value = 0;
};

struct LabelResource {
    int id = 0;
};

struct OwnedResource {
    int value = 0;
};

} // namespace

TEST_CASE("World resources insert and get values" * doctest::test_suite("fast")) {
    engine::World world;

    auto& resource = world.insert_resource(CounterResource{7});

    CHECK(resource.value == 7);
    CHECK(world.resource<CounterResource>().value == 7);
}

TEST_CASE("World resources return null for missing optional access" * doctest::test_suite("fast")) {
    engine::World world;

    CHECK(world.try_resource<CounterResource>() == nullptr);
}

TEST_CASE("World resources replace existing values" * doctest::test_suite("fast")) {
    engine::World world;
    world.insert_resource(CounterResource{1});

    auto& replacement = world.insert_resource(CounterResource{2});

    CHECK(replacement.value == 2);
    CHECK(world.resource<CounterResource>().value == 2);
}

TEST_CASE("World resources copy and own lvalue inserts" * doctest::test_suite("fast")) {
    engine::World world;
    OwnedResource original{12};

    auto& stored = world.insert_resource(original);
    original.value = 99;

    CHECK(stored.value == 12);
    CHECK(world.resource<OwnedResource>().value == 12);
}

TEST_CASE("World resource ids normalize cv and reference variants" * doctest::test_suite("fast")) {
    CHECK(engine::resource_id_for<OwnedResource>() ==
          engine::resource_id_for<const OwnedResource>());
    CHECK(engine::resource_id_for<OwnedResource>() == engine::resource_id_for<OwnedResource&>());
    CHECK(engine::resource_id_for<OwnedResource>() ==
          engine::resource_id_for<const OwnedResource&>());
}

TEST_CASE("World resources resolve cv and reference variants to the same value" *
          doctest::test_suite("fast")) {
    engine::World world;
    OwnedResource original{15};
    world.insert_resource(original);

    CHECK(world.resource<OwnedResource>().value == 15);
    CHECK(world.resource<const OwnedResource>().value == 15);
    CHECK(world.resource<OwnedResource&>().value == 15);
    CHECK(world.resource<const OwnedResource&>().value == 15);
    REQUIRE(world.try_resource<const OwnedResource&>() != nullptr);
    CHECK(world.try_resource<const OwnedResource&>()->value == 15);

    world.remove_resource<const OwnedResource&>();

    CHECK(world.try_resource<OwnedResource>() == nullptr);
}

TEST_CASE("World resources remove values" * doctest::test_suite("fast")) {
    engine::World world;
    world.insert_resource(CounterResource{3});

    world.remove_resource<CounterResource>();

    CHECK(world.try_resource<CounterResource>() == nullptr);
}

TEST_CASE("World resources ignore removal of missing values" * doctest::test_suite("fast")) {
    engine::World world;

    world.remove_resource<LabelResource>();

    CHECK(world.try_resource<LabelResource>() == nullptr);
}

TEST_CASE("World resources support const access" * doctest::test_suite("fast")) {
    engine::World world;
    world.insert_resource(CounterResource{9});
    const engine::World& const_world = world;

    const auto* resource = const_world.try_resource<CounterResource>();

    REQUIRE(resource != nullptr);
    CHECK(resource->value == 9);
    CHECK(const_world.resource<CounterResource>().value == 9);
}
