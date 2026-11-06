cmake_minimum_required(VERSION 3.13)
project(chatnav_host_tests C)

set(CMAKE_C_STANDARD 11)
enable_testing()

add_library(unity STATIC vendor/unity.c)
target_include_directories(unity PUBLIC vendor)

function(add_unity_test name)
    add_executable(${name} ${name}.c ${ARGN})
    target_link_libraries(${name} unity)
    target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../src)
    target_compile_options(${name} PRIVATE -Wall -Wextra)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

add_unity_test(test_config)
