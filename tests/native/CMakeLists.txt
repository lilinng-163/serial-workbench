# SPDX-License-Identifier: Apache-2.0

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
