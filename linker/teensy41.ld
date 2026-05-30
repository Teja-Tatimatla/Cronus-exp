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

  .isr_vector ALIGN(1024) : {
    KEEP(*(.isr_vector))
  } > FLASH

  .text : {
    *(.text*)
    *(.rodata*)
  } > FLASH

  .data : {
    _sdata = .;
    *(.data*)
    _edata = .;
  } > DTCM AT > FLASH
  _sidata = LOADADDR(.data);

  .bss : {
    _sbss = .;
    *(.bss*)
    *(COMMON)
    _ebss = .;
  } > DTCM

  _estack = ORIGIN(DTCM) + LENGTH(DTCM);

  _flash_image_len = LOADADDR(.data) + SIZEOF(.data) - ORIGIN(FLASH);
}
