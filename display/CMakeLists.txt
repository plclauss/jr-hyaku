# The typical stuff...
cmake_minimum_required(VERSION 4.2)

project(SVGDisplayer)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build/bin")
add_executable(SVGDisplayer)

# Dependency - GPIOD / libgpiod
find_package(PkgConfig REQUIRED)
pkg_check_modules(GPIOD REQUIRED IMPORTED_TARGET libgpiod)

# Dependency - nlohmann/json
include(FetchContent)
FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz)
FetchContent_MakeAvailable(json)

# More typical stuff... (compilation)
if(DEFINED DEBUG)
    message("Compiling DEBUG mode...")
    target_compile_definitions(SVGDisplayer PRIVATE LOGGER_ENABLE DEBUG)
endif()

if(DEFINED LOGGER_ENABLE)
    target_compile_definitions(SVGDisplayer PRIVATE LOGGER_ENABLE)
endif()

file(GLOB_RECURSE SRC_FILES
    src/*.c
    src/*.cpp
)
target_sources(SVGDisplayer PRIVATE ${SRC_FILES})
target_include_directories(SVGDisplayer PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(SVGDisplayer PRIVATE
    PkgConfig::GPIOD
    nlohmann_json::nlohmann_json
)
