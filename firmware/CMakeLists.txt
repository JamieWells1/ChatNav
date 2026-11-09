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

set(GPS_SOURCES
        src/gps/nmea.c
        src/gps/fix_record.c
        src/gps/fix_buffer.c
        src/gps/gps_uart.c
        src/gps/gps.c
)
set(INPUT_SOURCES src/input/input.c)
set(MIC_SOURCES src/audio/mic.c src/audio/pcm.c src/audio/volume.c src/audio/wav.c)
set(AMP_SOURCES src/audio/speaker.c src/audio/cues.c src/audio/pcm.c src/audio/volume.c
        src/audio/jitter.c)

# Every firmware image shares flags, includes and USB serial logging.
function(chatnav_firmware name)
    cmake_parse_arguments(FW "" "" "SOURCES;PIO;LIBS" ${ARGN})
    add_executable(${name} ${FW_SOURCES} src/common/error.c)
    pico_set_program_name(${name} "${name}")
    pico_set_program_version(${name} "0.1")
    pico_enable_stdio_uart(${name} 0)
    pico_enable_stdio_usb(${name} 1)
    target_compile_options(${name} PRIVATE
            -Wall
            -Wextra
            -Werror=return-type
            -Wno-unused-parameter
    )
    target_include_directories(${name} PRIVATE
            ${CMAKE_CURRENT_LIST_DIR}
            ${CMAKE_CURRENT_LIST_DIR}/src
    )
    target_link_libraries(${name} pico_stdlib ${FW_LIBS})
    foreach(pio_file ${FW_PIO})
        pico_generate_pio_header(${name} ${CMAKE_CURRENT_LIST_DIR}/pio/${pio_file})
    endforeach()
    pico_add_extra_outputs(${name})
endfunction()

set(SETUP_SOURCES
        src/setup/portal.c
        src/setup/portal_form.c
        src/setup/portal_html.c
        src/setup/portal_http.c
        src/setup/dns_server.c
        src/setup/dhcpserver.c
)
set(NET_SOURCES
        src/net/wifi.c
        src/net/fingerprint.c
        src/net/tls_client.c
        src/net/mbedtls_time.c
        src/net/http_resp.c
        src/net/http_req.c
        src/net/chunkq.c
        src/net/http_stream.c
        src/net/uplink.c
        src/net/fix_batch.c
        src/net/fix_sched.c
        src/net/fix_upload.c
        src/net/link.c
)

chatnav_firmware(chatnav
        SOURCES src/main.c src/status/led.c src/status/led_pattern.c
                src/app/app_state.c src/app/app_timing.c src/app/boot_hold.c src/app/voice.c
                ${INPUT_SOURCES} ${GPS_SOURCES}
                src/audio/mic.c src/audio/wav.c src/audio/playback.c ${AMP_SOURCES}
                src/storage/device_config.c src/storage/flash_memory.c
                ${SETUP_SOURCES} ${NET_SOURCES}
        PIO i2s_in.pio i2s_out.pio
        LIBS pico_cyw43_arch_lwip_threadsafe_background pico_lwip_mbedtls pico_mbedtls
             pico_flash hardware_flash hardware_watchdog hardware_gpio
             hardware_pio hardware_dma hardware_irq hardware_clocks hardware_uart
)

chatnav_firmware(diag_gps
        SOURCES diag/diag_gps.c ${GPS_SOURCES}
        LIBS hardware_uart hardware_irq
)
chatnav_firmware(diag_encoder
        SOURCES diag/diag_encoder.c ${INPUT_SOURCES}
        LIBS hardware_gpio
)
chatnav_firmware(diag_mic
        SOURCES diag/diag_mic.c ${MIC_SOURCES}
        PIO i2s_in.pio
        LIBS hardware_pio hardware_dma hardware_irq hardware_clocks
)
chatnav_firmware(diag_amp
        SOURCES diag/diag_amp.c ${AMP_SOURCES}
        PIO i2s_out.pio
        LIBS hardware_pio hardware_dma hardware_irq hardware_clocks
)
