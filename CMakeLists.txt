cmake_minimum_required(VERSION 3.21)
project(SerialWorkbench VERSION 0.7.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets Network SerialPort Test Core5Compat)

add_library(workbench_core native/core.cpp native/core.h native/mcp.cpp native/mcp.h)
target_link_libraries(workbench_core PUBLIC Qt6::Network Qt6::SerialPort Qt6::Core5Compat)
target_include_directories(workbench_core PUBLIC native)

qt_add_executable(SerialWorkbench WIN32 native/main.cpp native/window.cpp native/window.h native/controls.cpp native/controls.h native/terminal.cpp native/terminal.h)
target_link_libraries(SerialWorkbench PRIVATE workbench_core Qt6::Widgets)

qt_add_executable(SerialWorkbenchMCP native/bridge.cpp)
target_link_libraries(SerialWorkbenchMCP PRIVATE Qt6::Network)

enable_testing()
qt_add_executable(native_tests native/tests.cpp native/window.cpp native/window.h native/controls.cpp native/controls.h native/terminal.cpp native/terminal.h)
target_link_libraries(native_tests PRIVATE workbench_core Qt6::Widgets Qt6::Test)
foreach(target SerialWorkbench native_tests)
    qt_add_resources(${target} "workbench_icons" PREFIX "/" FILES native/check.svg native/chevron-up.svg native/chevron-down.svg)
endforeach()
set_target_properties(native_tests SerialWorkbenchMCP PROPERTIES WIN32_EXECUTABLE FALSE)
add_test(NAME native_tests COMMAND native_tests -platform offscreen)

foreach(target workbench_core SerialWorkbench SerialWorkbenchMCP native_tests)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /utf-8)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wno-unused-parameter)
    endif()
endforeach()
