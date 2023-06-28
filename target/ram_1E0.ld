ENTRY(_start)

/* Specify the memory areas */
MEMORY
{
  FLASH (rx)	: ORIGIN = 0x08000000, LENGTH = 64K
  RAM (xrw)	: ORIGIN = 0x20000000, LENGTH = 20K
}

SECTIONS
{
  /* The startup code goes first into FLASH */
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector)) /* Startup code */
    . = ALIGN(4);
    . = 0x1E0;
    *(.entryVec) /* entry vector position for SRAM execution */
  } >RAM

   
  /* The program code and other data goes into FLASH */
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
