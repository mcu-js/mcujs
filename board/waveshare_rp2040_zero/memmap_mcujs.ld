/*
 * mcujs - Custom Memory Map for Waveshare RP2040-Zero
 * 
 * Memory Layout:
 * +------------------+ 0x10000000 (XIP_BASE)
 * |    Boot2         | 256 bytes
 * +------------------+ 
 * |                  |
 * |  Program Code    |
 * |  (.text, .data,  |
 * |   .rodata, etc)  |
 * |                  |
 * +------------------+ __flash_binary_end (aligned to 4KB)
 * |                  |
 * |  Filesystem      |
 * |  (FAT12)         |
 * |                  |
 * +------------------+ _FS_end
 * |  EEPROM (4KB)    |
 * +------------------+ 0x10200000 (FLASH_END for 2MB)
 */

MEMORY
{
    FLASH(rx) : ORIGIN = 0x10000000, LENGTH = 2048k
    RAM(rwx) : ORIGIN = 0x20000000, LENGTH = 256k
    SCRATCH_X(rwx) : ORIGIN = 0x20040000, LENGTH = 4k
    SCRATCH_Y(rwx) : ORIGIN = 0x20041000, LENGTH = 4k
}

/* EEPROM size at end of flash */
__eeprom_size = 4k;

/* Flash end address */
__flash_end = ORIGIN(FLASH) + LENGTH(FLASH);

ENTRY(_entry_point)

SECTIONS
{
    /* Boot2 section - must be first */
    .flash_begin : {
        __flash_binary_start = .;
    } > FLASH

    .boot2 : {
        __boot2_start__ = .;
        KEEP (*(.boot2))
        __boot2_end__ = .;
    } > FLASH

    ASSERT(__boot2_end__ - __boot2_start__ == 256,
        "ERROR: Pico second stage bootloader must be 256 bytes in size")

    /* Main program code */
    .text : {
        __logical_binary_start = .;
        KEEP (*(.vectors))
        KEEP (*(.binary_info_header))
        __binary_info_header_end = .;
        KEEP (*(.reset))
        
        *(.init)
        *(.text*)
        *(.fini)
        
        /* C++ constructors/destructors */
        KEEP (*crtbegin.o(.ctors))
        KEEP (*(EXCLUDE_FILE (*crtend.o) .ctors))
        KEEP (*(SORT(.ctors.*)))
        KEEP (*crtend.o(.ctors))
        
        KEEP (*crtbegin.o(.dtors))
        KEEP (*(EXCLUDE_FILE (*crtend.o) .dtors))
        KEEP (*(SORT(.dtors.*)))
        KEEP (*crtend.o(.dtors))
        
        *(.eh_frame*)
        . = ALIGN(4);
    } > FLASH

    .rodata : {
        *(EXCLUDE_FILE(*libgcc.a: *libc.a:*lib_a-mem*.o *libm.a:) .rodata*)
        . = ALIGN(4);
        *(SORT_BY_ALIGNMENT(SORT_BY_NAME(.flashdata*)))
        . = ALIGN(4);
    } > FLASH

    /* Provide __etext symbol for runtime */
    __etext = .;

    /* Pre-init array (called before main) */
    .preinit_array : {
        . = ALIGN(4);
        __preinit_array_start = .;
        KEEP (*(.preinit_array*))
        __preinit_array_end = .;
    } > FLASH

    /* Init array (C++ constructors, etc.) */
    .init_array : {
        . = ALIGN(4);
        __init_array_start = .;
        KEEP (*(SORT(.init_array.*)))
        KEEP (*(.init_array*))
        __init_array_end = .;
    } > FLASH

    /* Fini array (C++ destructors, etc.) */
    .fini_array : {
        . = ALIGN(4);
        __fini_array_start = .;
        KEEP (*(SORT(.fini_array.*)))
        KEEP (*(.fini_array*))
        __fini_array_end = .;
    } > FLASH

    .ARM.extab :
    {
        *(.ARM.extab* .gnu.linkonce.armextab.*)
    } > FLASH

    __exidx_start = .;
    .ARM.exidx :
    {
        *(.ARM.exidx* .gnu.linkonce.armexidx.*)
    } > FLASH
    __exidx_end = .;

    /* Machine inspection data */
    . = ALIGN(4);
    __binary_info_start = .;
    .binary_info :
    {
        KEEP(*(.binary_info.keep.*))
        *(.binary_info.*)
    } > FLASH
    __binary_info_end = .;
    
    . = ALIGN(4);

    /* End of program code - this is where filesystem starts */
    .flash_end : {
        __flash_binary_end = .;
        /* Align to 4KB sector boundary for filesystem */
        . = ALIGN(4096);
        PROVIDE(_FS_start = .);
    } > FLASH

    /* RAM sections */
    .ram_vector_table (NOLOAD): {
        *(.ram_vector_table)
    } > RAM

    .data : {
        __data_start__ = .;
        *(vtable)
        *(.time_critical*)
        
        *(.rodata.str1.4*)
        *(EXCLUDE_FILE(*libgcc.a: *libc.a:*lib_a-mem*.o *libm.a:) .data*)
        . = ALIGN(4);
        *(.after_data.*)
        . = ALIGN(4);
        
        PROVIDE_HIDDEN (__mutex_array_start = .);
        KEEP(*(SORT(.mutex_array.*)))
        KEEP(*(.mutex_array))
        PROVIDE_HIDDEN (__mutex_array_end = .);

        . = ALIGN(4);
        __data_end__ = .;
    } > RAM AT> FLASH

    .uninitialized_data (NOLOAD): {
        . = ALIGN(4);
        *(.uninitialized_data*)
    } > RAM

    /* For C++ exception handling */
    .tdata : {
        . = ALIGN(4);
        *(.tdata .tdata.* .gnu.linkonce.td.*)
        . = ALIGN(4);
    } > RAM AT> FLASH

    .tbss (NOLOAD) : {
        *(.tbss .tbss.* .gnu.linkonce.tb.*)
        *(.tcommon)
    } > RAM

    .bss (NOLOAD) : {
        . = ALIGN(4);
        __bss_start__ = .;
        *(SORT_BY_ALIGNMENT(SORT_BY_NAME(.bss*)))
        *(COMMON)
        . = ALIGN(4);
        __bss_end__ = .;
    } > RAM

    .heap (NOLOAD):
    {
        __end__ = .;
        end = __end__;
        KEEP(*(.heap*))
        __HeapLimit = .;
    } > RAM

    /* Stack */
    .stack1_dummy (NOLOAD):
    {
        *(.stack1*)
    } > SCRATCH_X
    .stack_dummy (NOLOAD):
    {
        KEEP(*(.stack*))
    } > SCRATCH_Y

    .scratch_x : {
        __scratch_x_start__ = .;
        *(.scratch_x.*)
        . = ALIGN(4);
        __scratch_x_end__ = .;
    } > SCRATCH_X AT > FLASH
    __scratch_x_source__ = LOADADDR(.scratch_x);

    .scratch_y : {
        __scratch_y_start__ = .;
        *(.scratch_y.*)
        . = ALIGN(4);
        __scratch_y_end__ = .;
    } > SCRATCH_Y AT > FLASH
    __scratch_y_source__ = LOADADDR(.scratch_y);

    /* Filesystem end - before EEPROM reserved area */
    PROVIDE(_FS_end = __flash_end - __eeprom_size);
    
    /* EEPROM emulation area */
    PROVIDE(_EEPROM_start = _FS_end);

    /* Stack sizes */
    __StackLimit = ORIGIN(SCRATCH_Y);
    __StackOneTop = ORIGIN(SCRATCH_X) + LENGTH(SCRATCH_X);
    __StackTop = ORIGIN(SCRATCH_Y) + LENGTH(SCRATCH_Y);
    __StackOneBottom = __StackOneTop - SIZEOF(.stack1_dummy);
    __StackBottom = __StackTop - SIZEOF(.stack_dummy);

    PROVIDE(__stack = __StackTop);

    /* Symbols for runtime filesystem size calculation */
    PROVIDE(__flash_binary_size = __flash_binary_end - __flash_binary_start);
    PROVIDE(_FS_size = _FS_end - _FS_start);

    /* Check we have enough flash for program */
    ASSERT(__flash_binary_end <= _FS_start, 
        "ERROR: Program code overlaps filesystem region")
    
    /* Check filesystem has minimum size */
    ASSERT(_FS_size >= 65536, 
        "ERROR: Filesystem too small (minimum 64KB required)")
}
