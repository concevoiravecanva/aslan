# Test executable for Aslan
cmake_minimum_required(VERSION 3.10)

# Test runner executable
add_executable(test_runner
    test_runner.c
    ../src/document/document.c
    ../src/document/spatial_index.c
    ../src/core/engine.c
)

target_include_directories(test_runner PRIVATE
    ../include
    ${SDL2_INCLUDE_DIRS}
)

target_link_libraries(test_runner
    ${SDL2_LIBRARIES}
    m
)

# Add test command
enable_testing()
add_test(NAME AslanTests COMMAND test_runner)
