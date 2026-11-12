#include <stddef.h>
#include <stdint.h>

#include "board.h"
#include "device/image_crc.h"
#include "emmc.h"
#include "hal/system.h"

#define OVERLAY_BASE 0x0307A000u
#define OVERLAY_CAPACITY 0x18000u
#define OVERLAY_IMAGE_OFFSET 0x200000u
#define OVERLAY_MAGIC 0x4C564F45u
#define EXTRA_IMAGE_OFFSET 0x218000u
#define EXTRA_MAGIC 0x52545845u
#define THUMB_RETURN 0x4770u

enum {
    SECTOR_BYTES = 512,
    HEADER_BYTES = 16,
    SECTORS_PER_READ = 8,
    EXTRA_TRIES = 3
};

extern uint16_t device_extra_start[];
extern uint16_t device_extra_end[];

static uint32_t header[SECTOR_BYTES / 4];

static uint32_t le32(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8 | (uint32_t)bytes[2] << 16 |
           (uint32_t)bytes[3] << 24;
}

static bool read_overlay(uint8_t *target, uint32_t first_sector, uint32_t size)
{
    uint32_t sectors = (size + SECTOR_BYTES - 1) / SECTOR_BYTES;
    for (uint32_t done = 0; done < sectors; done += SECTORS_PER_READ) {
        uint32_t count = sectors - done < SECTORS_PER_READ ? sectors - done : SECTORS_PER_READ;
        if (!emmc_read(NULL, first_sector + done, count, target + done * SECTOR_BYTES)) {
            return false;
        }
    }
    return true;
}

const void *hal_system_load_rescan_code(void)
{
    EmmcInfo info;
    if (hal_system_restart_pending() || !emmc_start(&info)) {
        return NULL;
    }
    board_audio_shut_down();
    uint32_t first_sector = info.layout.slot_1 + OVERLAY_IMAGE_OFFSET / SECTOR_BYTES;
    if (!emmc_read(NULL, first_sector, 1, header)) {
        return NULL;
    }
    const uint8_t *bytes = (const uint8_t *)header;
    uint32_t size = le32(bytes + 4);
    uint32_t whole_sectors = (size + SECTOR_BYTES - 1) / SECTOR_BYTES * SECTOR_BYTES;
    if (le32(bytes) != OVERLAY_MAGIC || size < HEADER_BYTES || whole_sectors > OVERLAY_CAPACITY) {
        return NULL;
    }
    if (!read_overlay((uint8_t *)(uintptr_t)OVERLAY_BASE, first_sector, size)) {
        return NULL;
    }
    const uint8_t *overlay = (const uint8_t *)(uintptr_t)OVERLAY_BASE;
    if (image_crc_update(0, overlay + HEADER_BYTES, size - HEADER_BYTES) != le32(bytes + 8)) {
        return NULL;
    }
    __asm__ volatile("dsb\n\tisb" ::: "memory");
    return overlay + HEADER_BYTES;
}

static void fill_extra_with_returns(void)
{
    for (uint16_t *at = device_extra_start; at < device_extra_end; at++) {
        *at = THUMB_RETURN;
    }
}

static bool load_extra(uint32_t first_sector, uint8_t *extra, uint32_t capacity)
{
    const uint8_t *bytes = (const uint8_t *)header;
    return emmc_read(NULL, first_sector, 1, header) && le32(bytes) == EXTRA_MAGIC &&
           le32(bytes + 4) == capacity && read_overlay(extra, first_sector + 1, capacity) &&
           image_crc_update(0, extra, capacity) == le32(bytes + 8);
}

bool board_extra_start(void)
{
    EmmcInfo info;
    uint8_t *extra = (uint8_t *)device_extra_start;
    uint32_t capacity = (uint32_t)((uint8_t *)device_extra_end - extra);
    bool loaded = false;
    if (emmc_start(&info)) {
        uint32_t first_sector = info.layout.slot_1 + EXTRA_IMAGE_OFFSET / SECTOR_BYTES;
        for (int attempt = 0; attempt < EXTRA_TRIES && !loaded; attempt++) {
            loaded = load_extra(first_sector, extra, capacity);
        }
    }
    if (!loaded) {
        fill_extra_with_returns();
    }
    __asm__ volatile("dsb\n\tisb" ::: "memory");
    return loaded;
}
