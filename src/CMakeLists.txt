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

pico_add_extra_outputs(attack)