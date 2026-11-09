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
    target_compile_definitions(${name} PRIVATE FIXTURE_DIR="${CMAKE_CURRENT_LIST_DIR}/fixtures")
    add_test(NAME ${name} COMMAND ${name})
endfunction()

add_unity_test(test_config)
add_unity_test(test_nmea ../src/gps/nmea.c ../src/gps/fix_record.c)
add_unity_test(test_fix_buffer ../src/gps/fix_buffer.c ../src/gps/fix_record.c)
add_unity_test(test_pcm ../src/audio/pcm.c ../src/audio/volume.c ../src/audio/wav.c)
add_unity_test(test_volume ../src/audio/volume.c)
add_unity_test(test_cues ../src/audio/cues.c ../src/audio/pcm.c ../src/audio/volume.c)
add_unity_test(test_led_pattern ../src/status/led_pattern.c)
add_unity_test(test_device_config ../src/storage/device_config.c)
add_unity_test(test_fingerprint ../src/net/fingerprint.c)
add_unity_test(test_portal_form ../src/setup/portal_form.c ../src/storage/device_config.c
               ../src/net/fingerprint.c)
add_unity_test(test_http_resp ../src/net/http_resp.c)
add_unity_test(test_app_state ../src/app/app_state.c ../src/status/led_pattern.c)
add_unity_test(test_app_timing ../src/app/app_timing.c ../src/app/app_state.c
               ../src/status/led_pattern.c)
add_unity_test(test_jitter ../src/audio/jitter.c)
add_unity_test(test_fix_upload ../src/net/fix_batch.c ../src/net/fix_sched.c ../src/net/http_req.c
               ../src/gps/fix_buffer.c ../src/gps/fix_record.c)
add_unity_test(test_http_req ../src/net/http_req.c ../src/net/chunkq.c)

find_library(LIBM m)
if(LIBM)
    foreach(t test_pcm test_volume test_cues)
        target_link_libraries(${t} ${LIBM})
    endforeach()
endif()
