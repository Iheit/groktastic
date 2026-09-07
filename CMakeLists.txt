cmake_minimum_required(VERSION 3.20)
project(groktastic VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(raylib
  GIT_REPOSITORY https://github.com/raysan5/raylib.git
  GIT_TAG 5.5
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(raylib)

add_executable(groktastic src/main.cpp)
target_compile_definitions(groktastic PRIVATE RAYLIB_VERSION_MAJOR=5)
target_link_libraries(groktastic PRIVATE raylib)
if (MSVC)
  target_compile_options(groktastic PRIVATE /W4 /permissive-)
else()
  target_compile_options(groktastic PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
endif()

add_custom_command(TARGET groktastic POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:groktastic>/assets)
