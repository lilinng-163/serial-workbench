qt_add_executable(SerialWorkbench WIN32 main.cpp)
workbench_configure_target(SerialWorkbench)
target_link_libraries(SerialWorkbench PRIVATE workbench_ui)

install(TARGETS SerialWorkbench
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    BUNDLE DESTINATION .
)
