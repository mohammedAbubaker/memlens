cmake_minimum_required(VERSION 3.30)
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


include(FetchContent)
project(memlens LANGUAGES CXX C)
add_executable(treemap main.cpp)

FetchContent_Declare        (raylib GIT_REPOSITORY https://github.com/raysan5/raylib.git GIT_TAG 5.5)
FetchContent_MakeAvailable  (raylib)

target_link_libraries(treemap PRIVATE raylib)
