cmake_minimum_required(VERSION 3.6)

# --- Vcpkg automatic integration ---
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    if(EXISTS "$ENV{USERPROFILE}/vcpkg/scripts/buildsystems/vcpkg.cmake")
        set(CMAKE_TOOLCHAIN_FILE "$ENV{USERPROFILE}/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain file")
    elseif(EXISTS "C:/vcpkg/scripts/buildsystems/vcpkg.cmake")
        set(CMAKE_TOOLCHAIN_FILE "C:/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain file")
    endif()
endif()

if(NOT DEFINED VCPKG_TARGET_TRIPLET)
    if(TRUE)
        set(VCPKG_TARGET_TRIPLET "x64-mingw-static" CACHE STRING "Vcpkg target triplet")
    else()
        set(VCPKG_TARGET_TRIPLET "x64-windows" CACHE STRING "Vcpkg target triplet")
    endif()
endif()

# Explicit paths for Python on Windows
if(NOT DEFINED Python_INCLUDE_DIR)
    if(EXISTS "C:/Python311/include")
        set(Python_INCLUDE_DIR "C:/Python311/include" CACHE PATH "Python include directory")
    elseif(EXISTS "C:/Python310/include")
        set(Python_INCLUDE_DIR "C:/Python310/include" CACHE PATH "Python include directory")
    endif()
endif()

if(NOT DEFINED Python_LIBRARY)
    if(EXISTS "C:/Python311/libs/python311.lib")
        set(Python_LIBRARY "C:/Python311/libs/python311.lib" CACHE FILEPATH "Python library")
    elseif(EXISTS "C:/Python310/libs/python310.lib")
        set(Python_LIBRARY "C:/Python310/libs/python310.lib" CACHE FILEPATH "Python library")
    endif()
endif()

project(imvw C CXX)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)


if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif ()

if(MSVC)
    set(CMAKE_C_FLAGS "/W3")
    set(CMAKE_C_FLAGS_DEBUG "/Zi /Od")
    set(CMAKE_C_FLAGS_RELEASE "/O2")
else()
    set(CMAKE_C_FLAGS "-Wall -Wextra")
    set(CMAKE_C_FLAGS_DEBUG "-g")
    set(CMAKE_C_FLAGS_RELEASE "-O3")
endif()

include(FetchContent)

FetchContent_Declare(
        tinyexr
        GIT_REPOSITORY "https://github.com/syoyo/tinyexr.git"
        GIT_TAG "v1.0.1"
        GIT_SHALLOW TRUE
        UPDATE_DISCONNECTED TRUE
)
FetchContent_MakeAvailable(tinyexr)

set(WebP_USE_STATIC_LIBS ON)
set(TIFF_USE_STATIC_LIBS ON)
set(ZLIB_USE_STATIC_LIBS ON)

find_package(WebP CONFIG REQUIRED)
find_package(TIFF REQUIRED)
find_package(ZLIB REQUIRED)
find_package(JPEG REQUIRED)
find_package(LibLZMA REQUIRED)
find_package(Python REQUIRED COMPONENTS Development)

# --- trlib (local library) ---
add_subdirectory(../d3d11_bench trlib_build)

file(GLOB_RECURSE PROJECT_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_LIST_DIR}/sources/*.c"
)

set(PROJECT_INCLUDE "${CMAKE_CURRENT_LIST_DIR}/sources/")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

add_executable(${PROJECT_NAME}
        sources/main.c
        sources/imvw_interface.c
        sources/imvw_time.c
        sources/imvw_init.c
        sources/stb_image_impl.c
        sources/win_include.h
        sources/image_decoder.h
        resources.rc
)

target_sources(${PROJECT_NAME} PRIVATE ${PROJECT_SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
        ${PROJECT_INCLUDE}
        ${TIFF_INCLUDE_DIR}
        ${WEBP_INCLUDE_DIR}
        ${tinyexr_SOURCE_DIR}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
        trlib
        Python::Python
        winmm
        shlwapi
        TIFF::TIFF
        JPEG::JPEG
        LibLZMA::LibLZMA
        ws2_32
        ZLIB::ZLIB
        WebP::webp
        WebP::webpdecoder
        WebP::webpdemux
        WebP::libwebpmux
        WebP::sharpyuv
)

target_link_libraries(${PROJECT_NAME} PRIVATE WebP::webp WebP::webpdecoder WebP::webpdemux)
target_link_libraries(${PROJECT_NAME} PRIVATE WebP::libwebpmux)
target_link_libraries(${PROJECT_NAME} PRIVATE WebP::sharpyuv)

target_compile_definitions(${PROJECT_NAME}
        PUBLIC ASSETS_PATH="assets/"
)

set(ASSETS_DEST "${CMAKE_BINARY_DIR}/bin/assets")

#add_custom_command(
#        TARGET ${PROJECT_NAME} POST_BUILD
#        COMMAND ${CMAKE_COMMAND} -E copy_directory
#        "${CMAKE_CURRENT_SOURCE_DIR}/assets"
#        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/assets"
#        COMMENT "Copying assets to build directory..."
#)

# Copy trlib shaders (required by tr_raylib's InitWindow)
get_filename_component(TRLIB_SHADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../d3d11_bench" ABSOLUTE)

#add_custom_command(
#        TARGET ${PROJECT_NAME} POST_BUILD
#        COMMAND ${CMAKE_COMMAND} -E rm -f "$<TARGET_FILE_DIR:${PROJECT_NAME}>/shader_vs.hlsl"
#        COMMAND ${CMAKE_COMMAND} -E rm -f "$<TARGET_FILE_DIR:${PROJECT_NAME}>/shader_ps.hlsl"
#        COMMAND ${CMAKE_COMMAND} -E create_symlink
#        "${TRLIB_SHADER_DIR}/shader_vs.hlsl"
#        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/shader_vs.hlsl"
#        COMMAND ${CMAKE_COMMAND} -E create_symlink
#        "${TRLIB_SHADER_DIR}/shader_ps.hlsl"
#        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/shader_ps.hlsl"
#        COMMENT "Symlinking trlib shaders..."
#)
