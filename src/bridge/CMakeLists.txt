# SPDX-License-Identifier: Apache-2.0

qt_add_executable(SerialWorkbenchMCP main.cpp)
workbench_configure_target(SerialWorkbenchMCP)
target_link_libraries(SerialWorkbenchMCP PRIVATE Qt6::Network)

install(TARGETS SerialWorkbenchMCP RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
