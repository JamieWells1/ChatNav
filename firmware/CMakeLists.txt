cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()

set(PICO_BOARD pico2_w CACHE STRING "Board type")

include(pico_sdk_import.cmake)

project(chatnav C CXX ASM)

pico_sdk_init()

add_executable(chatnav
        src/main.c
        src/status/led.c
)

pico_set_program_name(chatnav "chatnav")
pico_set_program_version(chatnav "0.1")

pico_enable_stdio_uart(chatnav 0)
pico_enable_stdio_usb(chatnav 1)

target_compile_options(chatnav PRIVATE
        -Wall
        -Wextra
        -Werror=return-type
        -Wno-unused-parameter
)

target_include_directories(chatnav PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/src
)

target_link_libraries(chatnav
        pico_stdlib
        pico_cyw43_arch_lwip_threadsafe_background
)

pico_add_extra_outputs(chatnav)
