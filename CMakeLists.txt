cmake_minimum_required(VERSION 3.16)
project(ZSSK)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-fno-modules-ts)
set(CMAKE_EXPERIMENTAL_CXX_MODULE_CMAKE_API OFF)

include_directories(${CMAKE_SOURCE_DIR}/include)

add_executable(ZSSK
        src/main.cpp
        src/scheduler.cpp
        src/algorithms.cpp
        src/utils.cpp
)
