ENTRY(device_reset)

MEMORY
{
    DATA_RAM (rw) : ORIGIN = 0x03004000, LENGTH = 0x42A00
    EXTRA_RAM (rwx) : ORIGIN = 0x03046A00, LENGTH = 0x3800
    EXTRA_STORE (r) : ORIGIN = 0x10000000, LENGTH = 0x3800
    CODE_RAM (rx) : ORIGIN = 0x03050000, LENGTH = 0x2666C
    HIGH_RAM (rw) : ORIGIN = 0x0307A000, LENGTH = 0x18000
    GLYPH_RAM (rw) : ORIGIN = 0x03092000, LENGTH = 0xD000
}

SECTIONS
{
    .extra :
    {
        device_extra_start = .;
        *:info_screen.c.obj(.text .text.* .rodata .rodata.*)
        *:volume_blur.c.obj(.text .text.* .rodata .rodata.*)
        *:font_sample_screen.c.obj(.text .text.* .rodata .rodata.*)
        *:now_playing_screen.c.obj(.text .text.* .rodata .rodata.*)
        *:stopwatch_screen.c.obj(.text .text.* .rodata .rodata.*)
        *:test_pattern_screen.c.obj(.text .text.* .rodata .rodata.*)
        . = ALIGN(512);
        device_extra_end = .;
    } > EXTRA_RAM AT > EXTRA_STORE

    .code :
    {
        KEEP(*(.code_vectors))
        *(.text .text.*)
        *(.rodata .rodata.*)
        . = ALIGN(4);
    } > CODE_RAM

    .data :
    {
        KEEP(*(.entry_vector))
        *(.data .data.*)
        . = ALIGN(4);
    } > DATA_RAM AT > CODE_RAM

    .bss (NOLOAD) :
    {
        device_bss_start = .;
        *(.bss .bss.*)
        . = ALIGN(4);
        device_bss_end = .;
    } > DATA_RAM

    .noinit (NOLOAD) :
    {
        *(.noinit .noinit.*)
        . = ALIGN(4);
        device_noinit_end = .;
    } > DATA_RAM

    .high_ram (NOLOAD) :
    {
        *(.high_ram .high_ram.*)
        . = ALIGN(4);
    } > HIGH_RAM

    .glyph_ram (NOLOAD) :
    {
        *(.glyph_ram .glyph_ram.*)
        . = ALIGN(4);
    } > GLYPH_RAM

    .comment 0 : { *(.comment) }
    .ARM.attributes 0 : { *(.ARM.attributes) }

    /DISCARD/ :
    {
        *(.ARM.exidx*)
        *(.ARM.extab*)
    }
}

ASSERT(ADDR(.code) == 0x03050000, "the code vector table must be at 0x03050000")
ASSERT(ADDR(.data) == 0x03004000, "the entry vector must be at 0x03004000")
ASSERT(SIZEOF(.data) == 8, "only the entry vector may be in .data: make initialised globals const")
ASSERT(ADDR(.extra) == 0x03046A00, "the loaded code block must start at 0x03046A00")
ASSERT(device_noinit_end <= 0x0304A240, ".data, .bss and .noinit must end below 0x0304A240: the boot ROM clears this range while it still runs from RAM above it")
