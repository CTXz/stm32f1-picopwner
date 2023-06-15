add_executable(attack
    attack.c
)

# Add pico_stdlib library which aggregates commonly used features
target_link_libraries(attack pico_stdlib 
    hardware_uart 
    hardware_pwm
)

pico_enable_stdio_usb(attack 1)
pico_enable_stdio_uart(attack 0)

# Set the output directory for the UF2 file
set(OUTPUT_PATH "${CMAKE_CURRENT_BINARY_DIR}/..")
set_target_properties(attack PROPERTIES ARCHIVE_OUTPUT_DIRECTORY ${OUTPUT_PATH})

pico_add_extra_outputs(attack)