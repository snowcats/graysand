#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "emmc.h"
#include "hal/system.h"

enum {
    SECTOR_WORDS = 128,
    TAG_OFFSET = 0x30,
    TAG_LENGTH = 8
};

static const char VERSION_TEXT[] = PODIUM_VERSION_TEXT;
static const char HEX_DIGITS[] = "0123456789ABCDEF";
static char version[sizeof VERSION_TEXT + 1 + TAG_LENGTH];
static char hardware_text[9];
static bool tried;
static bool have_hardware;

enum {
    DATA_RAM_START = 0x03004000,
    DATA_RAM_END = 0x0304A240
};

extern char device_noinit_end[];

static bool is_printable(uint8_t value)
{
    return value >= 0x20 && value < 0x7F;
}

static void read_tag(void)
{
    static uint32_t header[SECTOR_WORDS];
    EmmcInfo info;
    if (!emmc_start(&info) || !emmc_read(NULL, info.layout.slot_1, 1, header)) {
        return;
    }
    have_hardware = true;
    for (int index = 0; index < 8; index++) {
        hardware_text[index] = HEX_DIGITS[(info.serial >> (28 - 4 * index)) & 0xF];
    }
    const uint8_t *bytes = (const uint8_t *)header + TAG_OFFSET;
    size_t length = sizeof VERSION_TEXT - 1;
    if (!is_printable(bytes[0])) {
        return;
    }
    version[length++] = '-';
    for (int index = 0; index < TAG_LENGTH && is_printable(bytes[index]); index++) {
        version[length++] = (char)bytes[index];
    }
}

const char *hal_system_version(void)
{
    if (!tried) {
        tried = true;
        for (size_t index = 0; index < sizeof VERSION_TEXT; index++) {
            version[index] = VERSION_TEXT[index];
        }
        read_tag();
    }
    return version;
}

const char *hal_system_hardware_version(void)
{
    if (!tried) {
        (void)hal_system_version();
    }
    return have_hardware ? hardware_text : "-";
}

HalMemory hal_system_memory(void)
{
    return (HalMemory){.used = (uint32_t)(uintptr_t)device_noinit_end - DATA_RAM_START,
                       .total = DATA_RAM_END - DATA_RAM_START};
}
