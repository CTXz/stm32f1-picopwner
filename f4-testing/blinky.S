@ Blinky in Assembly for the STM32F4

.equ RCC_AHB1ENR, 0x40023830
.equ GPIOC_MODER, 0x40020800
.equ GPIOC_ODR, 0x40020814
.equ GPIOC_BSR, 0x40020818

.syntax unified
.thumb

entry:

sp_addr: .word 0x20001000 	@ 0x00
.word 0x20000011		@ 0x04
.word 0x20000011		@ 0x08
.word 0x20000011		@ 0x0C

@ 0x10
_start:
	@ Set up the stack pointer
	ldr r0, =sp_addr
	ldr r0, [r0]
	mov sp, r0

	@ Initialize RCC for GPIOC
	ldr r0, =RCC_AHB1ENR 	@ RCC_AHB1ENR
	ldr r1, =0x4 		@ GPIOCEN
	str r1, [r0]		@ Apply to RCC_AHB1ENR

	@ Set GPIOC13 to output
	ldr r2, =GPIOC_MODER 	@ GPIOC_MODER
	ldr r3, =0x4000000	@ Output mode
	str r3, [r2]		@ Apply to GPIOC_MODER

	@ Set GPIOC13 to high
	ldr r3, =GPIOC_BSRR 	@ GPIOC_BSRR
	ldr r4, =0x2000		@ Set pin 13
	str r4, [r3]		@ Apply to GPIOC_BSRR

	ldr r3, =GPIOC_ODR 	@ GPIOC_ODR
loop:
	@ Blink the LED
	ldr r4, [r3]		@ Read GPIOC_ODR
	eor r4, r4, #0x2000	@ Toggle pin 13
	str r4, [r3]		@ Apply to GPIOC_ODR
	ldr r5, =0x100000
delay:
	subs r5, #1
	bne delay
	b loop

	@ End of program
	b .




