cmake_minimum_required(VERSION 3.13)

set(PICO_SDK_PATH "/usr/share/pico-sdk")

# initialize the SDK based on PICO_SDK_PATH
# note: this must happen before project()
include(pico_sdk_import.cmake)

project(pico_stm32_pwn C CXX ASM)

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

add_executable(attack
    attack.c
)

# Add pico_stdlib library which aggregates commonly used features
target_link_libraries(attack pico_stdlib)

pico_enable_stdio_usb(attack 1)
pico_enable_stdio_uart(attack 0)

# create map/bin/hex/uf2 file in addition to ELF.
pico_add_extra_outputs(attack)