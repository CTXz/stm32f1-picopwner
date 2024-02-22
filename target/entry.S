
@ Authors: JohannesObermaier, Patrick Pedersen
@ 
@ Target Firmware Version: 1.3
@
@ Entry points for stage 1 and stage 2 of the target
@ exploit firmware.
@ 
@ This code has been slightly modified from the original.
@ It provides better compatibility across the whole F1x family
@ and omits some uneccessary/unused code.
@
@ The original code can be found at:
@ 	https://github.com/JohannesObermaier/f103-analysis/tree/master/h3
@

.equ RCC_AHB1ENR, 0x40023830
.equ GPIOC_MODER, 0x40020800
.equ GPIOC_ODR, 0x40020814
.equ GPIOC_BSRR, 0x40020818

.global _start
.syntax unified
.thumb

@ -- Vector Table --
@ +1 Added to each jump address due to Thumb2
.section .vector_table
vtable:
sp_addr:.word 0x20001000 	@ 0x00: Stack Pointer address
	.word _start+1		@ 0x04: Reset Handler (Enters stage 1)
	.word isrNMI+1 		@ 0x08: NMI Handler
	.word _start+1		@ 0x0C: HardFault Handler
	.word isrMemMan+1 	@ 0x10: MemManage Handler
	.word 0			@ 0x14: BusFault not handled
	.word 0			@ 0x18: UsageFault not handled
	.word 0			@ 0x1CL Unused/Reserved
	.word stage2+1		@ 0x20: FPB reset remap

.section .text
.thumb
@ -- Stage 1 --
_start:
	@ Set FPB to remap the reset vector to SRAM
	ldr r0, =0xe0002000   @ FP_CTRL, see https://developer.arm.com/documentation/ddi0337/e/System-Debug/FPB/FPB-programmer-s-model
        movs r1, #3           @ FP_CTRL: Enable FPB                                                                        
        movs r2, #0x20        @ FP_REMAP: Remap to stage 2 vtable entry (0x20)                              
        movs r3, #0x05        @ FP_COMP0: Set COMP0 to (rst_vector | EN) = 0x05
        stm r0!, {r1, r2, r3} @ Apply to FPB                                                             
waitrst:
	b waitrst	      @ Wait for NRST pin to be toggled


@ -- Stage 2 --
stage2:	
	@ Set up the stack pointer
	ldr r0, =sp_addr
	ldr r0, [r0]
	mov sp, r0

	cpsie if @ Enable interrupts
	
	@ Configure VTOR
	ldr r0, =0xe000ED08 @ VTOR, see https://developer.arm.com/documentation/dui0552/a/cortex-m3-peripherals/system-control-block
	ldr r1, =vtable	    @ Address of the vector table
	str r1, [r0] 	    @ Set VTOR to point to our vector table  

	@ Clear BSS
	ldr r0, =_bss_end
	ldr r1, =_bss_start

	@ Check if there's even a BSS section to clear
	cmp r0, r1
	beq clrloop_done
	
	@ Cear the BSS section
	mov r3, #0
clrloop:
	strd r3, r3, [r1]
	add r1, r1, #8
	cmp r1, r0
	bne clrloop
clrloop_done:

	@ Jump to the main function defined in main.c
	bl main

	@ If main returns, we'll just loop forever
infloop:
	b infloop


@ -- Exception Handler --
@ Jump to alertCrash in case of either NMI, HardFault or MemManage
.section .text
isrNMI:
isrHardfault:
isrMemMan:
	mrs r0, IPSR @ Get the exception number
	b alertCrash @ Jump to alertCrash0