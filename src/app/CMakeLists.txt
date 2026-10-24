# SPDX-License-Identifier: Apache-2.0

qt_add_executable(SerialWorkbench WIN32 main.cpp)
workbench_configure_target(SerialWorkbench)
if(WIN32)
    configure_file("${PROJECT_SOURCE_DIR}/cmake/app.rc.in" "${CMAKE_CURRENT_BINARY_DIR}/app.rc" @ONLY)
    set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/app.rc" PROPERTIES
        OBJECT_DEPENDS "${PROJECT_SOURCE_DIR}/resources/brand/lightning.ico"
    )
    target_sources(SerialWorkbench PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/app.rc")
endif()
target_link_libraries(SerialWorkbench PRIVATE workbench_ui workbench_client)

qt_add_executable(SerialWorkbenchSSHAskpass ssh_askpass.cpp)
workbench_configure_target(SerialWorkbenchSSHAskpass)
target_link_libraries(SerialWorkbenchSSHAskpass PRIVATE Qt6::Network)
add_dependencies(SerialWorkbench SerialWorkbenchSSHAskpass)
add_dependencies(SerialWorkbench SerialWorkbenchSSHAskpass)

install(TARGETS SerialWorkbench SerialWorkbenchSSHAskpass
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    BUNDLE DESTINATION .
)
