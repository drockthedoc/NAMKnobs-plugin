# Standalone CI verification of the parametric-control contract, built against the SAME pinned
# NeuralAmpModelerCore submodule the plugin uses. No iPlug2 dependency. See verify_parametric.cpp.
cmake_minimum_required(VERSION 3.10)
project(namknobs_ci CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

set(CORE "${CMAKE_CURRENT_SOURCE_DIR}/../NeuralAmpModelerCore")
if(NOT EXISTS "${CORE}/NAM/get_dsp.h")
  message(FATAL_ERROR "NeuralAmpModelerCore submodule not found at ${CORE} (checkout with --recursive)")
endif()

file(GLOB NAM_SOURCES "${CORE}/NAM/*.cpp" "${CORE}/NAM/*/*.cpp")

add_executable(verify_parametric verify_parametric.cpp ${NAM_SOURCES})
target_include_directories(verify_parametric PRIVATE
  "${CORE}"
  "${CORE}/Dependencies/eigen"
  "${CORE}/Dependencies/nlohmann")

if(MSVC)
  target_compile_definitions(verify_parametric PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN _USE_MATH_DEFINES)
endif()
