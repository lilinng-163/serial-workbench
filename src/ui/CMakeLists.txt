# SPDX-License-Identifier: Apache-2.0

# Compile the UI once for the app and tests. Object linkage keeps Qt resource
# registration present in both executables.
add_library(workbench_ui OBJECT
    window.cpp
    window.h
    background_image.cpp
    background_image.h
    background_settings.cpp
    window_chrome.cpp
    window_chrome.h
    control_card.cpp
    control_card.h
    serial_monitor_dialog.cpp
    serial_monitor_dialog.h
    serial_control_panel.cpp
    serial_control_panel.h
    serial_control_presets.cpp
    serial_control_packet.cpp
    instrument_config.cpp
    instrument_config.h
    json_tools.cpp
    json_tools.h
    update_service.cpp
    update_service.h
    update_dialog.cpp
    update_dialog.h
    ssh_dialogs.cpp
    ssh_dialogs.h
    file_transfer_dialog.cpp
    file_transfer_dialog.h
    agent_config.cpp
    agent_config.h
    agent_panel.cpp
    agent_panel.h
    button_motion.cpp
    button_motion.h
    controls.cpp
    wave_plot.cpp
    wave_settings.cpp
    controls.h
    terminal.cpp
    terminal.h
    docking.cpp
    docking.h
    script_editor.cpp
    script_results.cpp
    parameter_grip.cpp
    parameter_grip.h
)
workbench_configure_target(workbench_ui)
target_include_directories(workbench_ui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(workbench_ui SYSTEM PRIVATE "${PROJECT_SOURCE_DIR}/third_party")
target_link_libraries(workbench_ui PUBLIC workbench_mcp Qt6::Widgets Qt6::Svg)

set(asset_root "${PROJECT_SOURCE_DIR}/resources")
qt_add_resources(workbench_ui workbench_assets
    PREFIX "/"
    BASE "${asset_root}"
    FILES
        "${asset_root}/agents/claude.svg"
        "${asset_root}/agents/gemini.svg"
        "${asset_root}/agents/grok.svg"
        "${asset_root}/agents/hermes.svg"
        "${asset_root}/agents/hermes-dark.svg"
        "${asset_root}/agents/codex.svg"
        "${asset_root}/agents/openclaw.svg"
        "${asset_root}/agents/opencode.svg"
        "${asset_root}/agents/opencode-dark.svg"
        "${asset_root}/agents/pi.svg"
        "${asset_root}/brand/lightning.svg"
        "${asset_root}/icons/check.svg"
        "${asset_root}/icons/chevron-up.svg"
        "${asset_root}/icons/chevron-down.svg"
        "${asset_root}/fonts/fusion-pixel-8px-monospaced-zh_hans.otf"
        "${asset_root}/fonts/fusion-pixel-12px-monospaced-zh_hans.otf"
        "${asset_root}/fonts/OFL.txt"
        "${asset_root}/translations/qtbase_zh_CN.qm"
)
