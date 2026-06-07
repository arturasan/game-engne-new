add_executable(clear_color main.cpp)
target_link_libraries(clear_color PRIVATE engine::engine)
set_target_properties(clear_color PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>/examples/clear_color"
)
