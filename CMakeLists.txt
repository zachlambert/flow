cmake_minimum_required(VERSION 3.14)
project(cpp-utils)

# Setup

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(FetchContent)

set(BUILD_ADDITIONAL_TARGETS OFF)
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(BUILD_ADDITIONAL_TARGETS ON)
endif()


# Dependencies

find_package(Threads REQUIRED)

FetchContent_Declare(cpp-utils
    GIT_REPOSITORY https://github.com/zachlambert/cpp-utils.git
    GIT_TAG master
)
FetchContent_MakeAvailable(cpp-utils)

FetchContent_Declare(parrot
    GIT_REPOSITORY https://github.com/zachlambert/parrot.git
    GIT_TAG master
)
FetchContent_MakeAvailable(parrot)

FetchContent_Declare(sentinel
    GIT_REPOSITORY https://github.com/zachlambert/sentinel.git
    GIT_TAG master
)
FetchContent_MakeAvailable(sentinel)


# Library

add_library(flow SHARED
    src/engine.cpp
    src/time.cpp
)
target_include_directories(flow PUBLIC include)
target_link_libraries(flow PUBLIC Threads::Threads cpp-utils parrot sentinel)


# Additional targets

if(BUILD_ADDITIONAL_TARGETS)
    add_executable(example_engine example/engine.cpp)
    target_link_libraries(example_engine flow)
endif()
