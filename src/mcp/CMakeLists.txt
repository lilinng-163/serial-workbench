# SPDX-License-Identifier: Apache-2.0

add_library(workbench_mcp STATIC
    mcp.cpp
    mcp.h
)
workbench_configure_target(workbench_mcp)
target_include_directories(workbench_mcp PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(workbench_mcp PUBLIC workbench_core Qt6::Network)
