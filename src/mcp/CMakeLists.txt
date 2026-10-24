# SPDX-License-Identifier: Apache-2.0

add_library(workbench_mcp STATIC
    mcp.cpp
    mcp.h
    script_tools.cpp
    usage_guide.cpp
    usage_guide.h
)
workbench_configure_target(workbench_mcp)
target_include_directories(workbench_mcp PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(workbench_mcp PUBLIC workbench_core Qt6::Network)

set(guide_root "${PROJECT_SOURCE_DIR}/skills/serial-workbench")
qt_add_resources(workbench_mcp workbench_guides
    PREFIX "/guides"
    BASE "${guide_root}"
    FILES "${guide_root}/SKILL.md" "${guide_root}/references/frame-script.md" "${guide_root}/references/cli.md"
)
install(DIRECTORY "${guide_root}" DESTINATION skills)
