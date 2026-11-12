#include "device/fat_space.h"

#include "fat_create.h"
#include "fat_layout.h"

enum {
    FIRST_CLUSTER = 2,
    BITS_PER_BYTE = 8,
    SECTORS_PER_MEGABYTE_SHIFT = 11,
    FAT32_ENTRY_MASK = 0x0FFFFFFF,
    INFO_STRUCT_OFFSET = 484,
    INFO_FREE_OFFSET = 488
};

#define INFO_LEAD_SIGNATURE 0x41615252u
#define INFO_STRUCT_SIGNATURE 0x61417272u

static uint32_t units_per_sector(const FatVolume *volume)
{
    switch (volume->type) {
    case FAT_TYPE_16:
        return FAT_SECTOR_SIZE / 2;
    case FAT_TYPE_32:
        return FAT_SECTOR_SIZE / 4;
    case FAT_TYPE_EXFAT:
        return FAT_SECTOR_SIZE * BITS_PER_BYTE;
    }
    return 0;
}

static bool unit_free(const FatVolume *volume, const uint8_t *bytes, uint32_t unit)
{
    switch (volume->type) {
    case FAT_TYPE_16:
        return fat_le16(bytes + unit * 2) == 0;
    case FAT_TYPE_32:
        return (fat_le32(bytes + unit * 4) & FAT32_ENTRY_MASK) == 0;
    case FAT_TYPE_EXFAT:
        return (bytes[unit / BITS_PER_BYTE] >> (unit % BITS_PER_BYTE) & 1) == 0;
    }
    return false;
}

static uint32_t run_at(FatVolume *volume, FatFile *bitmap, uint32_t sector, uint32_t limit,
                       uint32_t *lba)
{
    if (volume->type != FAT_TYPE_EXFAT) {
        *lba = volume->fat_start + sector;
        return limit;
    }
    return fat_file_sector(volume, bitmap, sector, lba)
               ? fat_run_length(volume, bitmap, sector, limit)
               : 0;
}

static bool info_free_clusters(FatVolume *volume, uint8_t *buffer, uint32_t *free_count)
{
    if (volume->info_sector == 0 ||
        !volume->device.read(volume->device.context, volume->info_sector, 1, buffer) ||
        fat_le32(buffer) != INFO_LEAD_SIGNATURE ||
        fat_le32(buffer + INFO_STRUCT_OFFSET) != INFO_STRUCT_SIGNATURE ||
        fat_le32(buffer + INFO_FREE_OFFSET) > volume->cluster_count) {
        return false;
    }
    *free_count = fat_le32(buffer + INFO_FREE_OFFSET);
    return true;
}

bool fat_free_clusters(FatVolume *volume, uint8_t *buffer, uint32_t sectors, uint32_t *free_count)
{
    if (volume->type == FAT_TYPE_32) {
        return sectors > 0 && info_free_clusters(volume, buffer, free_count);
    }
    uint32_t per_sector = units_per_sector(volume);
    FatFile bitmap = {0};
    bool exfat = volume->type == FAT_TYPE_EXFAT;
    if (per_sector == 0 || sectors == 0 || (exfat && !exfat_find_bitmap(volume, &bitmap))) {
        return false;
    }
    uint32_t first = exfat ? 0 : FIRST_CLUSTER;
    uint32_t end = first + volume->cluster_count;
    uint32_t total = (end + per_sector - 1) / per_sector;
    uint32_t count = 0;
    for (uint32_t sector = 0; sector < total;) {
        uint32_t lba = 0;
        uint32_t limit = total - sector < sectors ? total - sector : sectors;
        uint32_t run = run_at(volume, &bitmap, sector, limit, &lba);
        if (run == 0 || !volume->device.read(volume->device.context, lba, run, buffer)) {
            return false;
        }
        uint32_t base = sector * per_sector;
        uint32_t stop = (sector + run) * per_sector < end ? (sector + run) * per_sector : end;
        for (uint32_t unit = base < first ? first : base; unit < stop; unit++) {
            count += unit_free(volume, buffer, unit - base) ? 1u : 0u;
        }
        sector += run;
    }
    *free_count = count;
    return true;
}

uint32_t fat_cluster_megabytes(const FatVolume *volume, uint32_t clusters)
{
    return (uint32_t)(((uint64_t)clusters << volume->cluster_shift) >> SECTORS_PER_MEGABYTE_SHIFT);
}
