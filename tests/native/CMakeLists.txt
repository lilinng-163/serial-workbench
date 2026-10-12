qt_add_executable(native_tests tests.cpp)
workbench_configure_target(native_tests)
target_link_libraries(native_tests PRIVATE workbench_ui Qt6::Test)

add_test(NAME native_tests COMMAND native_tests
    -platform offscreen
    -o "${PROJECT_BINARY_DIR}/native-tests.txt,txt"
)
set_tests_properties(native_tests PROPERTIES TIMEOUT 60)
