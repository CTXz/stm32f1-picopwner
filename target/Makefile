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

# Makefile for building the target
# Targets:
#  - all: Build all target firmware binaries
#  - usart1: Build target firmware binaries for USART1
#  - usart2: Build target firmware binaries for USART2
#  - usart3: Build target firmware binaries for USART3
#  - clean: Remove all build files
#  - release: Create a release tarball containing all target firmware binaries

RELEASE_VERSION=1.4
BUILD_OUT=build
CC=arm-none-eabi-gcc
LD=arm-none-eabi-ld
OBJCOPY=arm-none-eabi-objcopy
CFLAGS=-mthumb -mcpu=cortex-m3 -Os -g3
ASMFLAGS=-mthumb -mcpu=cortex-m3 -g3
LDFLAGS=-nostartfiles -mcpu=cortex-m3 -mthumb -g3
LDSCRIPT=
DEFINES=
OBJECTS=main.o entry.o
OBJECTS_OUTPUT=$(addprefix $(BUILD_TARGET_OUT)/, $(OBJECTS))
ELFS=target.elf
ELFS_OUTPUT=$(addprefix $(BUILD_TARGET_OUT)/, $(ELFS))

usart1: DEFINES += -DUSE_USART1
usart1: BUILD_TARGET_OUT := $(BUILD_OUT)/usart1

usart2: DEFINES += -DUSE_USART2
usart2: BUILD_TARGET_OUT := $(BUILD_OUT)/usart2

usart3: DEFINES += -DUSE_USART3
usart3: BUILD_TARGET_OUT := $(BUILD_OUT)/usart3

default: all

create_build_dir:
	mkdir -p $(BUILD_OUT)

create_build_target_dir:
	mkdir -p $(BUILD_TARGET_OUT)

%.o: %.S create_build_dir create_build_target_dir
	$(CC) -o $(BUILD_TARGET_OUT)/$@ -c $< $(ASMFLAGS)

%.o: %.c create_build_dir create_build_target_dir
	$(CC) -o $(BUILD_TARGET_OUT)/$@ -c $< $(CFLAGS) $(DEFINES)

%.elf: $(OBJECTS) create_build_dir create_build_target_dir
	$(CC) -o $(BUILD_TARGET_OUT)/$@ $(OBJECTS_OUTPUT) $(LDFLAGS) -Tram.ld

usart1: $(OBJECTS) $(ELFS)
	$(OBJCOPY) -O binary $(BUILD_TARGET_OUT)/target.elf target_usart1.bin

usart2: $(OBJECTS) $(ELFS)
	$(OBJCOPY) -O binary $(BUILD_TARGET_OUT)/target.elf target_usart2.bin

usart3: $(OBJECTS) $(ELFS)
	$(OBJCOPY) -O binary $(BUILD_TARGET_OUT)/target.elf target_usart3.bin

all:
	$(MAKE) usart1
	$(MAKE) usart2
	$(MAKE) usart3

release: all
	tar -czvf target_firmware_v$(RELEASE_VERSION).tar.gz *.bin

clean:
	rm -f -r $(BUILD_OUT)
	rm -f *.bin *.tar.gz

.phony: default usart1 usart2 usart3 clean release
