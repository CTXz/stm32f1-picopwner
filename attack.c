#include "pico/stdlib.h"

// Power pins high
// Wait for button press <- GPIO INPUT
// Power pins low
// Wait for reset to go low <- GPIO INPUT
// Power pins high
// Wait for SRAM firmware to run (Approx 14.5ms) 
// Set boot0 pin and reset pin to low
// Wait for reset release (Approx 14.5ms)
// Set reset to input
// Wait for SRAM firmware to run (Approx 218.5ms)
// Set boot0 pin low again?

#define LED_PIN     PICO_DEFAULT_LED_PIN

#define POWER_PIN_1 2
#define POWER_PIN_2 3
#define RESET_PIN   4
#define BOOT0_PIN   5

#define BTN_PIN     24

int main() {
    gpio_init(LED_PIN);
    gpio_init(POWER_PIN_1);
    gpio_init(POWER_PIN_2);
    gpio_init(RESET_PIN);
    gpio_init(BOOT0_PIN);
    gpio_init(BTN_PIN);
    
    while (true) {

        gpio_set_dir(LED_PIN, GPIO_OUT);
        gpio_set_dir(POWER_PIN_1, GPIO_OUT);
        gpio_set_dir(POWER_PIN_2, GPIO_OUT);
        gpio_set_dir(BOOT0_PIN, GPIO_OUT);
        gpio_set_dir(RESET_PIN, GPIO_IN);
        gpio_set_dir(BTN_PIN, GPIO_IN);
        gpio_pull_up(BTN_PIN);

        gpio_put(POWER_PIN_1, 1);
        gpio_put(POWER_PIN_2, 1);
        gpio_put(BOOT0_PIN, 1);
        gpio_put(LED_PIN, 0);

        sleep_ms(500);

        while(gpio_get(BTN_PIN)) {
            tight_loop_contents();
        }

        gpio_put(LED_PIN, 1);
        gpio_put(POWER_PIN_1, 0);
        gpio_put(POWER_PIN_2, 0);

        while(gpio_get(RESET_PIN)) {
            tight_loop_contents();
        }

        gpio_put(POWER_PIN_1, 1);
        gpio_put(POWER_PIN_2, 1);

        sleep_ms(15);

        gpio_put(BOOT0_PIN, 0);
        gpio_set_dir(RESET_PIN, GPIO_OUT);
        gpio_put(RESET_PIN, 0);

        sleep_ms(15);

        gpio_set_dir(RESET_PIN, GPIO_IN);

        sleep_ms(218);

        gpio_put(LED_PIN, 0);

    }
}
