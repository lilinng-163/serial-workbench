# SPDX-License-Identifier: Apache-2.0

add_executable(wave_benchmark wave_benchmark.cpp)
workbench_configure_target(wave_benchmark)
target_link_libraries(wave_benchmark PRIVATE workbench_ui Qt6::Test)

qt_add_executable(native_tests tests.cpp)
add_executable(fake_ssh fake_ssh.cpp)
if(MINGW)
    # A ConPTY fixture must also launch without SDK runtime DLL lookup.
    target_link_options(fake_ssh PRIVATE -static)
endif()
add_dependencies(native_tests SerialWorkbenchSSHAskpass fake_ssh)
workbench_configure_target(native_tests)
target_link_libraries(native_tests PRIVATE workbench_ui Qt6::Test)

get_filename_component(workbench_compiler_bin "${CMAKE_CXX_COMPILER}" DIRECTORY)
set(workbench_test_path "$<TARGET_FILE_DIR:Qt6::Core>;${workbench_compiler_bin};$ENV{PATH}")

add_test(NAME native_tests COMMAND "${CMAKE_COMMAND}" -E env
    "PATH=${workbench_test_path}"
    $<TARGET_FILE:native_tests>
    -platform offscreen
    -o "${PROJECT_BINARY_DIR}/native-tests.txt,txt"
)
set_tests_properties(native_tests PROPERTIES TIMEOUT 60)

qt_add_executable(frame_script_tests script_tests.cpp)
workbench_configure_target(frame_script_tests)
target_link_libraries(frame_script_tests PRIVATE workbench_ui Qt6::Test)

add_test(NAME frame_script_tests COMMAND "${CMAKE_COMMAND}" -E env
    "PATH=${workbench_test_path}"
    $<TARGET_FILE:frame_script_tests>
    -platform offscreen
    -o "${PROJECT_BINARY_DIR}/script-tests.txt,txt"
)
set_tests_properties(frame_script_tests PROPERTIES TIMEOUT 60)

qt_add_executable(agent_tests agent_tests.cpp)
workbench_configure_target(agent_tests)
target_link_libraries(agent_tests PRIVATE workbench_ui Qt6::Test)
add_test(NAME agent_tests COMMAND "${CMAKE_COMMAND}" -E env
    "PATH=${workbench_test_path}"
    $<TARGET_FILE:agent_tests>
    -platform offscreen
    -o "${PROJECT_BINARY_DIR}/agent-tests.txt,txt"
)
set_tests_properties(agent_tests PROPERTIES TIMEOUT 60)

qt_add_executable(serial_transfer_tests serial_transfer_tests.cpp)
workbench_configure_target(serial_transfer_tests)
target_link_libraries(serial_transfer_tests PRIVATE workbench_ui Qt6::Test)
add_test(NAME serial_transfer_tests COMMAND "${CMAKE_COMMAND}" -E env
    "PATH=${workbench_test_path}"
    $<TARGET_FILE:serial_transfer_tests>
    -platform offscreen
    -o "${PROJECT_BINARY_DIR}/serial-transfer-tests.txt,txt"
)
set_tests_properties(serial_transfer_tests PROPERTIES TIMEOUT 60)

qt_add_executable(mcp_protocol_tests mcp_protocol_tests.cpp)
workbench_configure_target(mcp_protocol_tests)
target_link_libraries(mcp_protocol_tests PRIVATE workbench_ui Qt6::Test)
add_test(NAME mcp_protocol_tests COMMAND "${CMAKE_COMMAND}" -E env
    "PATH=${workbench_test_path}"
    $<TARGET_FILE:mcp_protocol_tests>
    -platform offscreen
    -o "${PROJECT_BINARY_DIR}/mcp-protocol-tests.txt,txt"
)
set_tests_properties(mcp_protocol_tests PROPERTIES TIMEOUT 30)

qt_add_executable(ssh_auth_tests ssh_auth_tests.cpp)
workbench_configure_target(ssh_auth_tests)
target_link_libraries(ssh_auth_tests PRIVATE workbench_ui Qt6::Test)
add_dependencies(ssh_auth_tests SerialWorkbenchSSHAskpass fake_ssh)
add_test(NAME ssh_auth_tests COMMAND "${CMAKE_COMMAND}" -E env
    "PATH=${workbench_test_path}"
    $<TARGET_FILE:ssh_auth_tests>
    -platform offscreen
    -o "${PROJECT_BINARY_DIR}/ssh-auth-tests.txt,txt"
)
set_tests_properties(ssh_auth_tests PROPERTIES TIMEOUT 30)

# Deliberately no install() rule: the virtual wire belongs only to test/demo builds.
qt_add_executable(FileTransferDemo file_transfer_demo.cpp)
workbench_configure_target(FileTransferDemo)
target_link_libraries(FileTransferDemo PRIVATE workbench_ui)

# Internal SSH device simulator: build on request, never install or release.
qt_add_executable(SshNewDeviceDemo WIN32 ssh_new_device_demo.cpp)
workbench_configure_target(SshNewDeviceDemo)
target_compile_definitions(SshNewDeviceDemo PRIVATE WORKBENCH_DEMO_ROOT="${PROJECT_SOURCE_DIR}")
target_link_libraries(SshNewDeviceDemo PRIVATE workbench_ui)
add_dependencies(SshNewDeviceDemo SerialWorkbenchSSHAskpass)

qt_add_executable(wave_settings_tests wave_settings_tests.cpp)
workbench_configure_target(wave_settings_tests)
target_link_libraries(wave_settings_tests PRIVATE workbench_ui Qt6::Test)
add_test(NAME wave_settings_tests COMMAND "${CMAKE_COMMAND}" -E env
    "PATH=${workbench_test_path}"
    $<TARGET_FILE:wave_settings_tests>
    -platform offscreen
    -o "${PROJECT_BINARY_DIR}/wave-settings-tests.txt,txt"
)
set_tests_properties(wave_settings_tests PROPERTIES TIMEOUT 30)

qt_add_executable(json_tools_tests json_tools_tests.cpp)
workbench_configure_target(json_tools_tests)
target_link_libraries(json_tools_tests PRIVATE workbench_ui Qt6::Test)
add_test(NAME json_tools_tests COMMAND "${CMAKE_COMMAND}" -E env
    "PATH=${workbench_test_path}"
    $<TARGET_FILE:json_tools_tests>
    -platform offscreen
    -o "${PROJECT_BINARY_DIR}/json-tools-tests.txt,txt"
)
set_tests_properties(json_tools_tests PROPERTIES TIMEOUT 30)

qt_add_executable(dialog_resize_tests dialog_resize_tests.cpp)
workbench_configure_target(dialog_resize_tests)
target_link_libraries(dialog_resize_tests PRIVATE workbench_ui Qt6::Test)
add_dependencies(dialog_resize_tests fake_ssh)
add_test(NAME dialog_resize_tests COMMAND "${CMAKE_COMMAND}" -E env
    "PATH=${workbench_test_path}"
    $<TARGET_FILE:dialog_resize_tests>
    -platform offscreen
    -o "${PROJECT_BINARY_DIR}/dialog-resize-tests.txt,txt"
)
set_tests_properties(dialog_resize_tests PROPERTIES TIMEOUT 30)

qt_add_executable(terminal_keyboard_tests terminal_keyboard_tests.cpp)
workbench_configure_target(terminal_keyboard_tests)
target_link_libraries(terminal_keyboard_tests PRIVATE workbench_ui Qt6::Test)
add_test(NAME terminal_keyboard_tests COMMAND "${CMAKE_COMMAND}" -E env
    "PATH=${workbench_test_path}"
    $<TARGET_FILE:terminal_keyboard_tests>
    -platform offscreen
    -o "${PROJECT_BINARY_DIR}/terminal-keyboard-tests.txt,txt"
)
set_tests_properties(terminal_keyboard_tests PROPERTIES TIMEOUT 30)

qt_add_executable(background_tests background_tests.cpp)
workbench_configure_target(background_tests)
target_link_libraries(background_tests PRIVATE workbench_ui Qt6::Test)
add_test(NAME background_tests COMMAND "${CMAKE_COMMAND}" -E env
    "PATH=${workbench_test_path}"
    $<TARGET_FILE:background_tests>
    -platform offscreen
    -o "${PROJECT_BINARY_DIR}/background-tests.txt,txt"
)
set_tests_properties(background_tests PROPERTIES TIMEOUT 30)

qt_add_executable(window_chrome_tests window_chrome_tests.cpp)
workbench_configure_target(window_chrome_tests)
target_link_libraries(window_chrome_tests PRIVATE workbench_ui Qt6::Test)
add_test(NAME window_chrome_tests COMMAND "${CMAKE_COMMAND}" -E env
    "PATH=${workbench_test_path}"
    $<TARGET_FILE:window_chrome_tests>
    -platform offscreen
    -o "${PROJECT_BINARY_DIR}/window-chrome-tests.txt,txt"
)
set_tests_properties(window_chrome_tests PROPERTIES TIMEOUT 30)

qt_add_executable(serial_control_tests serial_control_tests.cpp)
workbench_configure_target(serial_control_tests)
target_link_libraries(serial_control_tests PRIVATE workbench_ui Qt6::Test)
add_test(NAME serial_control_tests COMMAND "${CMAKE_COMMAND}" -E env
    "PATH=${workbench_test_path}"
    $<TARGET_FILE:serial_control_tests>
    -platform offscreen
    -o "${PROJECT_BINARY_DIR}/serial-control-tests.txt,txt"
)
set_tests_properties(serial_control_tests PROPERTIES TIMEOUT 45)

qt_add_executable(button_motion_tests button_motion_tests.cpp)
workbench_configure_target(button_motion_tests)
target_link_libraries(button_motion_tests PRIVATE workbench_ui Qt6::Test)
add_test(NAME button_motion_tests COMMAND "${CMAKE_COMMAND}" -E env
    "PATH=${workbench_test_path}"
    $<TARGET_FILE:button_motion_tests>
    -platform offscreen
    -o "${PROJECT_BINARY_DIR}/button-motion-tests.txt,txt"
)
set_tests_properties(button_motion_tests PROPERTIES TIMEOUT 30)

qt_add_executable(search_filter_tests search_filter_tests.cpp)
workbench_configure_target(search_filter_tests)
target_link_libraries(search_filter_tests PRIVATE workbench_ui Qt6::Test)
add_test(NAME search_filter_tests COMMAND "${CMAKE_COMMAND}" -E env
    "PATH=${workbench_test_path}"
    $<TARGET_FILE:search_filter_tests>
    -platform offscreen
    -o "${PROJECT_BINARY_DIR}/search-filter-tests.txt,txt"
)
set_tests_properties(search_filter_tests PROPERTIES TIMEOUT 30)

qt_add_executable(mcp_workflow_tests mcp_workflow_tests.cpp)
workbench_configure_target(mcp_workflow_tests)
target_link_libraries(mcp_workflow_tests PRIVATE workbench_ui Qt6::Test)
add_test(NAME mcp_workflow_tests COMMAND "${CMAKE_COMMAND}" -E env
    "PATH=${workbench_test_path}"
    $<TARGET_FILE:mcp_workflow_tests>
    -platform offscreen
    -o "${PROJECT_BINARY_DIR}/mcp-workflow-tests.txt,txt"
)
set_tests_properties(mcp_workflow_tests PROPERTIES TIMEOUT 30)
