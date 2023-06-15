cmake_minimum_required(VERSION 3.13)

set(PICO_SDK_PATH "/usr/share/pico-sdk")

# initialize the SDK based on PICO_SDK_PATH
# note: this must happen before project()
include(pico_sdk_import.cmake)

project(attack C CXX ASM)

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

add_subdirectory(
    src
)