MEMORY {
  FLASH     (rx)  : ORIGIN = 0x60000000, LENGTH = 1984k
  ITCM      (rwx) : ORIGIN = 0x00000000, LENGTH = 512k
  DTCM      (rwx) : ORIGIN = 0x20000000, LENGTH = 512k
  RAM       (rwx) : ORIGIN = 0x20200000, LENGTH = 512k
}

ENTRY(Reset_Handler)

SECTIONS {
  .image : {
    KEEP(*(.flash_config))
    . = ORIGIN(FLASH) + 0x1000;
    KEEP(*(.ivt))
    KEEP(*(.boot_data))
  } > FLASH

  .bootcode : {
    . = ALIGN(4);
    KEEP(*(.bootcode))
    . = ALIGN(4);
  } > FLASH      /* Reset_Handler + copy loop: runs XIP */

  .isr_vector : {
    . = ALIGN(1024);
    _svector = .;
    KEEP(*(.isr_vector))
    . = ALIGN(4);
    _evector = .;
  } > ITCM AT > FLASH
  _sivector = LOADADDR(.isr_vector);

  .text : {
    . = ALIGN(4);
    _stext = .;
    *(.text*)

    . = ALIGN(4);
    __device_pre_start = .;
    KEEP(*(SORT_BY_NAME(.device_init.0_*)))
    __device_pre_end = .;

    . = ALIGN(4);
    __device_post_start = .;
    KEEP(*(SORT_BY_NAME(.device_init.1_*)))
    __device_post_end = .;

    *(.rodata*)
    . = ALIGN(4);
    _etext = .;
  } > ITCM AT > FLASH
  _sitext = LOADADDR(.text);

  .data : {
    . = ALIGN(4);
    _sdata = .;
    *(.data*)
    . = ALIGN(4);
    _edata = .;
  } > DTCM AT > FLASH
  _sidata = LOADADDR(.data);

  .bss : {
    . = ALIGN(4);
    _sbss = .;
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;
  } > DTCM

  _estack = ORIGIN(DTCM) + LENGTH(DTCM);

  _flash_image_len = LOADADDR(.data) + SIZEOF(.data) - ORIGIN(FLASH);
}
