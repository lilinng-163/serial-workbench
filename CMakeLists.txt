# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.21)

project(SerialWorkbench
    VERSION 0.15.14
    DESCRIPTION "Native serial and network workbench with shared MCP access"
    LANGUAGES CXX
)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "Use an out-of-source build: cmake -S . -B build/native")
endif()

include(CTest)
include(GNUInstallDirs)
include(cmake/ProjectOptions.cmake)

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets Network SerialPort Core5Compat Svg)
if(BUILD_TESTING)
    find_package(Qt6 6.5 REQUIRED COMPONENTS Test)
endif()

configure_file(cmake/workbench_version.h.in generated/workbench_version.h @ONLY)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(src)
if(BUILD_TESTING)
    add_subdirectory(tests/native)
endif()

install(FILES resources/fonts/OFL.txt resources/fonts/README.md
    DESTINATION licenses/FusionPixel
)
install(DIRECTORY resources/fonts/LICENSES DESTINATION licenses/FusionPixel)

install(FILES LICENSE DESTINATION .)

install(FILES resources/agents/README.md resources/agents/CC0.txt DESTINATION licenses/AgentLogos)
install(FILES third_party/tomlplusplus/LICENSE DESTINATION licenses/tomlplusplus)
install(FILES third_party/fkyaml/LICENSE DESTINATION licenses/fkYAML)
