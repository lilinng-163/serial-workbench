# SPDX-License-Identifier: Apache-2.0

add_library(workbench_options INTERFACE)
target_compile_features(workbench_options INTERFACE cxx_std_17)

if(MSVC)
    target_compile_options(workbench_options INTERFACE /W4 /utf-8)
else()
    target_compile_options(workbench_options INTERFACE
        -Wall
        -Wextra
        -Wno-unused-parameter
    )
endif()

function(workbench_configure_target target)
    target_link_libraries(${target} PRIVATE workbench_options)
    set_target_properties(${target} PROPERTIES
        AUTOMOC ON
        CXX_EXTENSIONS OFF
    )
endfunction()
