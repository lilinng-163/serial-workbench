# SPDX-License-Identifier: Apache-2.0

add_library(workbench_mcp STATIC
    mcp.cpp
    mcp.h
    script_tools.cpp
    openocd_tools.cpp
    usage_guide.cpp
    usage_guide.h
)
workbench_configure_target(workbench_mcp)
target_include_directories(workbench_mcp PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(workbench_mcp PUBLIC workbench_core Qt6::Network)

set(guide_root "${PROJECT_SOURCE_DIR}/skills/serial-workbench")
file(GLOB_RECURSE guide_files CONFIGURE_DEPENDS "${guide_root}/*.md")
qt_add_resources(workbench_mcp workbench_guides
    PREFIX "/guides"
    BASE "${guide_root}"
    FILES ${guide_files}
)
foreach(guide IN LISTS guide_files)
    file(RELATIVE_PATH relative "${guide_root}" "${guide}")
    get_filename_component(directory "${relative}" DIRECTORY)
    install(FILES "${guide}" DESTINATION "skills/serial-workbench/${directory}")
endforeach()
