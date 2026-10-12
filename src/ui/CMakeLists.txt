# Compile the UI once for the app and tests. Object linkage keeps Qt resource
# registration present in both executables.
add_library(workbench_ui OBJECT
    window.cpp
    window.h
    controls.cpp
    controls.h
    terminal.cpp
    terminal.h
    docking.cpp
    docking.h
)
workbench_configure_target(workbench_ui)
target_include_directories(workbench_ui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(workbench_ui PUBLIC workbench_mcp Qt6::Widgets)

set(asset_root "${PROJECT_SOURCE_DIR}/resources")
qt_add_resources(workbench_ui workbench_assets
    PREFIX "/"
    BASE "${asset_root}"
    FILES
        "${asset_root}/icons/check.svg"
        "${asset_root}/icons/chevron-up.svg"
        "${asset_root}/icons/chevron-down.svg"
        "${asset_root}/fonts/fusion-pixel-8px-monospaced-zh_hans.otf"
        "${asset_root}/fonts/fusion-pixel-12px-monospaced-zh_hans.otf"
        "${asset_root}/fonts/OFL.txt"
)
