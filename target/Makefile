default: usart1

usart1:
	arm-none-eabi-gcc -o test.o -c test.S -mthumb -mcpu=cortex-m3 -g3
	arm-none-eabi-gcc -o main.o -c main.c -DUSE_USART1 -mthumb -mcpu=cortex-m3 -Os -g3
	arm-none-eabi-gcc -o sram.elf ./test.o ./main.o -nostartfiles -Tram.ld -mcpu=cortex-m3 -mthumb -g3
	arm-none-eabi-objcopy -O binary sram.elf target.bin

usart2:
	arm-none-eabi-gcc -o test.o -c test.S -mthumb -mcpu=cortex-m3 -g3
	arm-none-eabi-gcc -o main.o -c main.c -DUSE_USART2 -mthumb -mcpu=cortex-m3 -Os -g3
	arm-none-eabi-gcc -o sram.elf ./test.o ./main.o -nostartfiles -Tram.ld -mcpu=cortex-m3 -mthumb -g3
	arm-none-eabi-objcopy -O binary sram.elf target.bin

usart3:
	arm-none-eabi-gcc -o test.o -c test.S -mthumb -mcpu=cortex-m3 -g3
	arm-none-eabi-gcc -o main.o -c main.c -DUSE_USART3 -mthumb -mcpu=cortex-m3 -Os -g3
	arm-none-eabi-gcc -o sram.elf ./test.o ./main.o -nostartfiles -Tram.ld -mcpu=cortex-m3 -mthumb -g3
	arm-none-eabi-objcopy -O binary sram.elf target.bin

clean:
	rm -f *.o *.elf *.bin