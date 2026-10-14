# SPDX-License-Identifier: Apache-2.0

add_executable(wave_benchmark wave_benchmark.cpp)
workbench_configure_target(wave_benchmark)
target_link_libraries(wave_benchmark PRIVATE workbench_ui Qt6::Test)

qt_add_executable(native_tests tests.cpp)
add_executable(fake_ssh fake_ssh.cpp)
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

# Deliberately no install() rule: the virtual wire belongs only to test/demo builds.
qt_add_executable(FileTransferDemo file_transfer_demo.cpp)
workbench_configure_target(FileTransferDemo)
target_link_libraries(FileTransferDemo PRIVATE workbench_ui)
