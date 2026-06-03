add_library(engine)
add_library(engine::engine ALIAS engine)

find_package(spdlog CONFIG REQUIRED)
find_package(SDL3 CONFIG REQUIRED)

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
        core/spdlog_backend.cpp
        platform/detail/sdl3_window_backend.cpp
        platform/events.cpp
        platform/input.cpp
        platform/platform.cpp
        platform/window.cpp
)

target_link_libraries(engine PRIVATE SDL3::SDL3 spdlog::spdlog)

target_sources(engine
    PUBLIC FILE_SET headers TYPE HEADERS BASE_DIRS ${CMAKE_SOURCE_DIR}
    FILES
        core/app.hpp
        core/log.hpp
        core/plugin.hpp
        core/result.hpp
        core/schedule.hpp
        ecs/entity.hpp
        ecs/events.hpp
        ecs/world.hpp
        ecs/detail/archetype.hpp
        ecs/detail/component_id.hpp
        platform/events.hpp
        platform/input.hpp
        platform/platform.hpp
        platform/types.hpp
        platform/window.hpp
        platform/detail/window_backend.hpp
)
