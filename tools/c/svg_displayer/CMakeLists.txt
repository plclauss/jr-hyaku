cmake_minimum_required(VERSION 4.2)

project(SVGDisplayer)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GPIOD REQUIRED IMPORTED_TARGET libgpiod)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/Build/bin")
add_executable(SVGDisplayer)

set(TARGET_PLATFORM "LINUX" CACHE STRING "Target platform")
set_property(CACHE TARGET_PLATFORM PROPERTY STRINGS LINUX ARDUINO)
if(TARGET_PLATFORM STREQUAL "LINUX")
    target_compile_definitions(SVGDisplayer PRIVATE LINUX)
else()
    message(FATAL_ERROR "Unknown TARGET_PLATFORM: ${TARGET_PLATFORM}")
endif()

file(GLOB_RECURSE SRC_FILES
    Src/*.c
    Src/*.cpp
)
target_sources(SVGDisplayer PRIVATE ${SRC_FILES})
target_include_directories(SVGDisplayer PRIVATE ${CMAKE_SOURCE_DIR}/Src)
target_link_libraries(SVGDisplayer PRIVATE PkgConfig::GPIOD)
