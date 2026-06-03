add_executable(hello_window main.cpp)
target_link_libraries(hello_window PRIVATE engine::engine)
set_target_properties(hello_window PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>/examples/hello_window"
)
