cmake_minimum_required(VERSION 3.16)
project(VeloxServe LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler warnings
add_compile_options(-Wall -Wextra -Wpedantic)

# Auto-discover all source files
file(GLOB_RECURSE SOURCES "src/*.cpp")

add_executable(veloxserve ${SOURCES})

target_include_directories(veloxserve PRIVATE include)

# Link pthreads
find_package(Threads REQUIRED)
target_link_libraries(veloxserve Threads::Threads)

# Release optimization
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(veloxserve PRIVATE -O2)
endif()
