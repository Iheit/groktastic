cmake_minimum_required(VERSION 3.20)
project(groktastic VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

# Keep raylib's own optional targets disabled so the Windows build only
# produces the game we need.
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
set(SHARED OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
  raylib
  GIT_REPOSITORY https://github.com/raysan5/raylib.git
  GIT_TAG 5.5
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(raylib)

add_executable(groktastic WIN32 src/main.cpp)
target_link_libraries(groktastic PRIVATE raylib)

target_compile_definitions(groktastic PRIVATE
  $<$<CONFIG:Release>:NDEBUG>
)

if(MSVC)
  target_compile_options(groktastic PRIVATE /W4 /permissive- /utf-8)
  target_compile_definitions(groktastic PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
  target_compile_options(groktastic PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/assets")
  add_custom_command(
    TARGET groktastic POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/assets"
            "$<TARGET_FILE_DIR:groktastic>/assets"
  )
endif()

# Make a portable Windows package directly from the Release build tree.
set(PACKAGE_DIR "${CMAKE_BINARY_DIR}/package")
add_custom_target(package_windows
  COMMAND ${CMAKE_COMMAND} -E rm -rf "${PACKAGE_DIR}"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${PACKAGE_DIR}"
  COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:groktastic>" "${PACKAGE_DIR}/groktastic.exe"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/assets" "${PACKAGE_DIR}/assets"
  DEPENDS groktastic
  COMMENT "Packaging Groktastic for Windows"
)
