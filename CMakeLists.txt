cmake_minimum_required(VERSION 3.6)

# 1. Change project language to CXX (C++)
# We also keep C enabled just in case you want to leave some legacy files as .c
project(imvw C CXX)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Adding Raylib
include(FetchContent)
set(FETCHCONTENT_QUIET FALSE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif ()

# 3. Update Flags to use CMAKE_C_FLAGS
set(CMAKE_C_FLAGS "-Wall -Wextra")
set(CMAKE_C_FLAGS_DEBUG "-g")
set(CMAKE_C_FLAGS_RELEASE "-O3")

FetchContent_Declare(
        raylib
        GIT_REPOSITORY "https://github.com/raysan5/raylib.git"
        GIT_TAG "5.0" # 1. Pin to a specific release (e.g., 5.0) instead of a branch
        GIT_SHALLOW TRUE # 2. Only download the specific commit, not the whole Git history
        GIT_PROGRESS TRUE
        UPDATE_DISCONNECTED TRUE # 3. Skip network updates if the folder already exists
        SYSTEM
)
FetchContent_Declare(
        tinyexr
        GIT_REPOSITORY "https://github.com/syoyo/tinyexr.git"
        GIT_TAG "v1.0.1"
        GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(tinyexr)

set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(raylib)

set(WebP_USE_STATIC_LIBS ON)
set(TIFF_USE_STATIC_LIBS ON)
set(ZLIB_USE_STATIC_LIBS ON)

find_package(WebP CONFIG REQUIRED)
find_package(TIFF REQUIRED)
find_package(ZLIB REQUIRED)

find_package(Python REQUIRED COMPONENTS Development)
find_package(WebP CONFIG REQUIRED)
find_package(TIFF REQUIRED)
find_package(ZLIB REQUIRED)

# -------------------------------

# MANUALLY INSTALL PYTHON DEV

set(PYTHON_VERSION "3.13.0")
set(PYTHON_ZIP_URL "https://www.python.org/ftp/python/${PYTHON_VERSION}/python-${PYTHON_VERSION}-embed-amd64.zip")

include(FetchContent)
FetchContent_Declare(
        python_runtime
        URL ${PYTHON_ZIP_URL}
)
FetchContent_MakeAvailable(python_runtime)

# Point CMake to the headers and libs inside the downloaded package
set(Python_INCLUDE_DIRS "${python_runtime_SOURCE_DIR}")
set(Python_LIBRARIES "${python_runtime_SOURCE_DIR}/python313.lib")

include_directories(${Python_INCLUDE_DIRS})

FetchContent_MakeAvailable(raylib)

# 4. Update GLOB to look for .cpp files (and .c if you are transitioning slowly)
file(GLOB_RECURSE PROJECT_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_LIST_DIR}/sources/*.c"
        #        "${CMAKE_CURRENT_LIST_DIR}/sources/*.cpp"
)

set(PROJECT_INCLUDE "${CMAKE_CURRENT_LIST_DIR}/sources/")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

add_executable(${PROJECT_NAME}
        #        WIN32
        sources/main.c
        sources/imvw_interface.c
        sources/imvw_time.c
        sources/imvw_init.c
        sources/win_include.h
        sources/image_decoder.h
        resources.rc
)

target_sources(${PROJECT_NAME} PRIVATE ${PROJECT_SOURCES})

# Include directories for Raylib, WebP, and TIFF
target_include_directories(${PROJECT_NAME} PRIVATE
        ${PROJECT_INCLUDE}
        ${raylib_SOURCE_DIR}/src/external/glfw/include
        ${TIFF_INCLUDE_DIR}
        ${WEBP_INCLUDE_DIR}
        ${tinyexr_SOURCE_DIR}
)


# Link the libraries!
target_link_libraries(${PROJECT_NAME} PRIVATE
        raylib
        Python::Python
        shlwapi
        TIFF::TIFF
        ws2_32
        ZLIB::ZLIB
        WebP::webp
        WebP::webpdecoder
        WebP::webpdemux
        WebP::libwebpmux
        WebP::sharpyuv
)
# Missing ====
# VCRUNTIME140D.dll
# ucrtbased.dll

target_link_libraries(${PROJECT_NAME} PRIVATE WebP::webp WebP::webpdecoder WebP::webpdemux)
target_link_libraries(${PROJECT_NAME} PRIVATE WebP::libwebpmux)
target_link_libraries(${PROJECT_NAME} PRIVATE WebP::sharpyuv)

#target_link_libraries(${PROJECT_NAME} PRIVATE zlib)

target_compile_definitions(${PROJECT_NAME}
        PUBLIC ASSETS_PATH="assets/"
)

set(ASSETS_DEST "${CMAKE_BINARY_DIR}/bin/assets")

add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/assets"
        COMMENT "Copying assets to build directory..."
)

