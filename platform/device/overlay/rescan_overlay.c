#include <stddef.h>

#include "device/block_device.h"
#include "device/fat_space.h"
#include "hal/storage.h"
#include "hal/system.h"
#include "library/cover_build.h"
#include "library/library_scan.h"

enum {
    SPACE_BUFFER_SECTORS = 64
};

bool sd_start(BlockDevice *device);
FatVolume *internal_volume_mount(void);
const LibraryScanEntry *rescan_overlay_entry(void);

static const char SPACE_PATH[] = "/.echo/rescan_times.bin";

static bool beating_read(void *context, uint32_t lba, uint32_t count, void *buffer)
{
    const BlockDevice *device = context;
    hal_system_keep_alive();
    return device->read(device->context, lba, count, buffer);
}

static HalSpace measured(FatVolume *volume, BlockDevice *device, uint8_t *buffer, uint32_t sectors)
{
    volume->device = (BlockDevice){beating_read, device, NULL};
    uint32_t free_count = 0;
    if (!fat_free_clusters(volume, buffer, sectors, &free_count)) {
        return (HalSpace){0, 0};
    }
    return (HalSpace){fat_cluster_megabytes(volume, volume->cluster_count - free_count),
                      fat_cluster_megabytes(volume, volume->cluster_count)};
}

static void measure_space(void *memory, uint32_t size)
{
    FatVolume *copy = memory;
    uint8_t *buffer = (uint8_t *)(copy + 1);
    if (size < sizeof *copy + SPACE_BUFFER_SECTORS * FAT_SECTOR_SIZE) {
        return;
    }
    HalSpace space[HAL_SPACE_COUNT] = {{0, 0}, {0, 0}};
    FatVolume *internal = internal_volume_mount();
    BlockDevice device;
    if (internal != NULL) {
        device = internal->device;
        *copy = *internal;
        space[HAL_SPACE_INTERNAL] = measured(copy, &device, buffer, SPACE_BUFFER_SECTORS);
    }
    if (hal_file_volume() == HAL_VOLUME_CARD && sd_start(&device) &&
        fat_mount(copy, (BlockDevice){device.read, device.context, NULL})) {
        space[HAL_SPACE_CARD] = measured(copy, &device, buffer, SPACE_BUFFER_SECTORS);
    }
    (void)hal_file_write_at(SPACE_PATH, HAL_SPACE_OFFSET, space, sizeof space);
}

static ScanResult scan_with_space(const ScanJob *job, ScanStats *stats)
{
    measure_space(job->memory, job->size);
    return library_scan(job, stats);
}

const LibraryScanEntry LIBRARY_SCAN_ENTRY = {
    .magic = LIBRARY_SCAN_MAGIC,
    .scan = scan_with_space,
    .covers = cover_build,
};

const LibraryScanEntry *rescan_overlay_entry(void)
{
    return &LIBRARY_SCAN_ENTRY;
}
