add_library(workbench_core STATIC
    core.cpp
    core.h
    ssh_profiles.cpp
    ssh_profiles.h
)
workbench_configure_target(workbench_core)
target_include_directories(workbench_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${PROJECT_BINARY_DIR}/generated"
)
target_link_libraries(workbench_core PUBLIC
    Qt6::Network
    Qt6::SerialPort
    Qt6::Core5Compat
)
