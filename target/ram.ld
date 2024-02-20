ENTRY(_start)

/* Specify the memory areas */
MEMORY
{
  FLASH (rx)	: ORIGIN = 0x08000000, LENGTH = 64K
  RAM_ISR (xrw)	: ORIGIN = 0x20000000, LENGTH = 768                                     /* Reserve 0x000-0x300 for ISR and entry point */
  RAM (xrw)	: ORIGIN = ORIGIN(RAM_ISR)+LENGTH(RAM_ISR), LENGTH = 4K-LENGTH(RAM_ISR)     /* Limit RAM usage to 4KB for best compatibility */
}

SECTIONS
{
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector))                    /* Startup code */
    . = ALIGN(4);
    . = 0x100;
    *(.entryVec)                            /* entry vector position for SRAM execution */
  } >RAM_ISR

  .text :
  {
	. = ALIGN(8);
	*(.text)           /* .text sections (code) */
	*(.text*)          /* .text* sections (code) */
  } >RAM

  . = ALIGN(0x100);
  .data :
  {
	. = ALIGN(8);
	*(.data)
	*(.data*)
  } >RAM

  .bss :
  {
	. = ALIGN(8);
	_bss_start = .;
	*(.bss)
	*(.bss*)
	. = ALIGN(8);
	_bss_end = .;
  } >RAM
}
