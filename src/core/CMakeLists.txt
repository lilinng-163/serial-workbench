# SPDX-License-Identifier: Apache-2.0

add_library(workbench_core STATIC
    core.cpp
    core.h
    ssh_profiles.cpp
    ssh_profiles.h
    ssh_process.cpp
    ssh_process.h
    ssh_auth.cpp
    ssh_auth.h
    frame_script.cpp
    frame_script.h
    script_worker.cpp
    serial_transfer.cpp
    serial_transfer.h
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
if(WIN32)
    target_link_libraries(workbench_core PRIVATE crypt32)
endif()
