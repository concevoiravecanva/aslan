cmake_minimum_required(VERSION 3.10)
project(Aslan VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Options
option(ASLAN_NO_LUA "Build without Lua support" OFF)

# Sources
set(SOURCES
    src/main.c
    src/document/document.c
    src/platform/platform.c
    src/scripting/lua_host.c
)

# Include directories
include_directories(src)

# Lua Dependency
if(NOT ASLAN_NO_LUA)
    find_package(Lua)
    if(LUA_FOUND)
        include_directories(${LUA_INCLUDE_DIR})
        list(APPEND LIBS ${LUA_LIBRARIES})
        add_definitions(-DHAS_LUA)
    else()
        message(WARNING "Lua not found. Building with NO_LUA defined.")
        add_definitions(-DNO_LUA)
    endif()
else()
    add_definitions(-DNO_LUA)
endif()

# Executable
add_executable(aslan ${SOURCES})

# Link libraries
if(LIBS)
    target_link_libraries(aslan ${LIBS})
endif()

# Math library for Linux/Unix
if(UNIX AND NOT APPLE)
    target_link_libraries(aslan m)
endif()
