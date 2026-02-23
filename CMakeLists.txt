cmake_minimum_required(VERSION 3.12)

# 1. Change project language to CXX (C++)
# We also keep C enabled just in case you want to leave some legacy files as .c
project(imvw C CXX)

# 2. Set C++ Standard (17 or 20 is recommended for modern features)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Adding Raylib
include(FetchContent)
set(FETCHCONTENT_QUIET FALSE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)

set(CMAKE_BUILD_TYPE Debug)

# 3. Update Flags to use CMAKE_CXX_FLAGS
set(CMAKE_CXX_FLAGS "-Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

FetchContent_Declare(
        raylib
        GIT_REPOSITORY "https://github.com/raysan5/raylib.git"
        GIT_TAG "5.0" # 1. Pin to a specific release (e.g., 5.0) instead of a branch
        GIT_SHALLOW TRUE # 2. Only download the specific commit, not the whole Git history
        GIT_PROGRESS TRUE
        UPDATE_DISCONNECTED TRUE # 3. Skip network updates if the folder already exists
)

find_package(Python REQUIRED COMPONENTS Development)


include_directories(${Python_INCLUDE_DIRS})
FetchContent_MakeAvailable(raylib)

# 4. Update GLOB to look for .cpp files (and .c if you are transitioning slowly)
file(GLOB_RECURSE PROJECT_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_LIST_DIR}/sources/*.c"
        "${CMAKE_CURRENT_LIST_DIR}/sources/*.cpp"
)

set(PROJECT_INCLUDE "${CMAKE_CURRENT_LIST_DIR}/sources/")

add_executable(${PROJECT_NAME}
        sources/main.cpp
        sources/imvw_interface.cpp
        sources/imvw_time.cpp
        sources/imvw_init.cpp
        sources/win_include.h
        # Note: You might want to keep system compat files as C
)

target_sources(${PROJECT_NAME} PRIVATE ${PROJECT_SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
        ${PROJECT_INCLUDE}
        ${raylib_SOURCE_DIR}/src/external/glfw/include
)

target_link_libraries(${PROJECT_NAME} PRIVATE raylib PRIVATE Python::Python PRIVATE shlwapi)

target_compile_definitions(${PROJECT_NAME}
        PUBLIC ASSETS_PATH="${CMAKE_CURRENT_SOURCE_DIR}/assets/")
