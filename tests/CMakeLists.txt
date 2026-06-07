find_package(doctest CONFIG REQUIRED)

add_executable(engine_tests
    unit/test_app.cpp
    unit/test_ecs.cpp
    unit/test_events.cpp
    unit/test_log.cpp
    unit/test_platform.cpp
    unit/test_renderer.cpp
    unit/test_resources.cpp
    unit/test_result.cpp
)

target_link_libraries(engine_tests PRIVATE engine::engine doctest::doctest)
target_compile_definitions(engine_tests PRIVATE ENGINE_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

include(${doctest_DIR}/doctest.cmake OPTIONAL RESULT_VARIABLE _doctest_module_found)
if(_doctest_module_found)
    doctest_discover_tests(engine_tests ADD_LABELS 1)
else()
    add_test(NAME engine_tests COMMAND engine_tests)
    set_tests_properties(engine_tests PROPERTIES LABELS "fast")
endif()
