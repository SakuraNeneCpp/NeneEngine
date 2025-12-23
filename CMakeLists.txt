cmake_minimum_required(VERSION 3.20)

project(NeneEngine
  VERSION 1.0.0
  LANGUAGES CXX
)

# ----------------------------
# Global settings
# ----------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ----------------------------
# SDL3 dependencies
# ----------------------------
# Prefer Config packages (vcpkg / system install that provides *Config.cmake)
find_package(SDL3 CONFIG REQUIRED)
find_package(SDL3_image CONFIG REQUIRED)
find_package(SDL3_ttf CONFIG REQUIRED)

# Helper: pick a usable target name if the package provides multiple variants
function(_pick_target outvar)
  foreach(tgt IN LISTS ARGN)
    if(TARGET "${tgt}")
      set(${outvar} "${tgt}" PARENT_SCOPE)
      return()
    endif()
  endforeach()
  message(FATAL_ERROR "None of these targets exist: ${ARGN}")
endfunction()

_pick_target(SDL3_LIB
  SDL3::SDL3
  SDL3::SDL3-shared
  SDL3::SDL3-static
)

# SDL3main is not always provided on every platform/setup, so link it if it exists
set(SDL3MAIN_LIB "")
if(TARGET SDL3::SDL3main)
  set(SDL3MAIN_LIB SDL3::SDL3main)
endif()

_pick_target(SDL3_IMAGE_LIB
  SDL3_image::SDL3_image
  SDL3_image::SDL3_image-shared
  SDL3_image::SDL3_image-static
)

_pick_target(SDL3_TTF_LIB
  SDL3_ttf::SDL3_ttf
  SDL3_ttf::SDL3_ttf-shared
  SDL3_ttf::SDL3_ttf-static
)

# ----------------------------
# NeneEngine library
# ----------------------------
add_library(NeneEngineLib STATIC
  src/NeneNode.cpp
  src/NeneServer.cpp
)

target_include_directories(NeneEngineLib
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(NeneEngineLib
  PUBLIC
    ${SDL3_LIB}
    ${SDL3_IMAGE_LIB}
    ${SDL3_TTF_LIB}
)

# ----------------------------
# ChromeDino executable
# ----------------------------
add_executable(ChromeDino
  ChromeDino/main.cpp
  ChromeDino/game.cpp
)

add_custom_command(TARGET ChromeDino POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_SOURCE_DIR}/ChromeDino/assets
          $<TARGET_FILE_DIR:ChromeDino>/assets
)


target_link_libraries(ChromeDino
  PRIVATE
    NeneEngineLib
    ${SDL3MAIN_LIB}
)

# On some environments you may want to copy runtime DLLs next to the exe.
# (vcpkg often handles this; otherwise do it manually if needed.)
