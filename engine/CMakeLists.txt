add_library(engine)
add_library(engine::engine ALIAS engine)

target_compile_features(engine PUBLIC cxx_std_26)

target_include_directories(engine
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
)

if(ENGINE_UNITY_BUILD)
    set_target_properties(engine PROPERTIES UNITY_BUILD ON)
endif()

target_sources(engine
    PRIVATE
        core/app.cpp
        core/schedule.cpp
)

target_sources(engine
    PUBLIC FILE_SET headers TYPE HEADERS BASE_DIRS ${CMAKE_SOURCE_DIR}
    FILES
        core/app.hpp
        core/plugin.hpp
        core/schedule.hpp
        ecs/entity.hpp
        ecs/world.hpp
        ecs/detail/archetype.hpp
        ecs/detail/component_id.hpp
)
