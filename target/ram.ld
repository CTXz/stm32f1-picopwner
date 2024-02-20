ENTRY(_start)

MEMORY
{
  FLASH (rx)	      : ORIGIN = 0x08000000, LENGTH = 64K
  RAM_VTABLE (xrw)	: ORIGIN = 0x20000000, LENGTH = 768                                     /* Reserve 0x000-0x300 for ISR and entry point */
  RAM (xrw)	        : ORIGIN = ORIGIN(RAM_VTABLE)+LENGTH(RAM_VTABLE), LENGTH = 4K-LENGTH(RAM_VTABLE)     /* Limit RAM usage to 4KB for best compatibility */
}

SECTIONS
{
  /* Vector Table and SRAM execution entry point */
  .vector_table :
  {
    . = ALIGN(4);
    KEEP(*(.vector_table))
    . = ALIGN(4);
    . = 0x100;
    *(.sram_entry)  /* Entry point for SRAM execution */
  } >RAM_VTABLE

  .text :
  {
	. = ALIGN(8);
	*(.text)
	*(.text*)
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
