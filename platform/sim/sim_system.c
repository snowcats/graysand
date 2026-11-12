#include "hal/storage.h"
#include "hal/system.h"
#include "library/library_scan.h"

enum {
    BOOT_CLOCK_MHZ = 24,
    SIM_INTERNAL_USED_MB = 10035,
    SIM_INTERNAL_TOTAL_MB = 16384,
    SIM_CARD_USED_MB = 217088,
    SIM_CARD_TOTAL_MB = 262144
};

static const char SIM_SPACE_PATH[] = "/.echo/rescan_times.bin";

static uint32_t clock_mhz = BOOT_CLOCK_MHZ;

const char *hal_system_version(void)
{
    return "simulator";
}

const char *hal_system_hardware_version(void)
{
    return "sim";
}

HalMemory hal_system_memory(void)
{
    return (HalMemory){.used = 0, .total = 0};
}

HalBootClocks hal_system_boot_clocks(void)
{
    return (HalBootClocks){0};
}

HalUsbCore hal_system_usb_core(void)
{
    return (HalUsbCore){.captured = false};
}

HalUsbLast hal_system_usb_last(void)
{
    return (HalUsbLast){.end = HAL_USB_END_NONE};
}

void hal_system_set_dac_volume(uint8_t level)
{
    (void)level;
}

void hal_system_set_usb_mode(HalUsbMode mode)
{
    (void)mode;
}

uint32_t hal_system_clock_mhz(void)
{
    return clock_mhz;
}

bool hal_system_set_clock_mhz(uint32_t mhz)
{
    clock_mhz = mhz;
    return true;
}

bool hal_system_restart_pending(void)
{
    return false;
}

static ScanResult scan_with_space(const ScanJob *job, ScanStats *stats)
{
    static const HalSpace space[HAL_SPACE_COUNT] = {
        [HAL_SPACE_INTERNAL] = {SIM_INTERNAL_USED_MB, SIM_INTERNAL_TOTAL_MB},
        [HAL_SPACE_CARD] = {SIM_CARD_USED_MB, SIM_CARD_TOTAL_MB}};
    (void)hal_file_write_at(SIM_SPACE_PATH, HAL_SPACE_OFFSET, space, sizeof space);
    return LIBRARY_SCAN_ENTRY.scan(job, stats);
}

const void *hal_system_load_rescan_code(void)
{
    static LibraryScanEntry entry;
    entry = LIBRARY_SCAN_ENTRY;
    entry.scan = scan_with_space;
    return &entry;
}

HalHighRamTest hal_system_high_ram_test(void)
{
    return (HalHighRamTest){.status = HAL_HIGH_RAM_UNAVAILABLE};
}

void *hal_system_borrow_scratch(uint32_t *size)
{
    static uint8_t scratch[384 * 1024];
    *size = sizeof scratch;
    return scratch;
}

void *hal_system_borrow_listing(uint32_t *size)
{
    static uint8_t listing[96 * 1024];
    *size = sizeof listing;
    return listing;
}

void hal_system_return_scratch(void)
{
}

void hal_system_stall_test(void)
{
}

void hal_system_keep_alive(void)
{
}

HalLoopCycles hal_system_loop_cycles(void)
{
    return (HalLoopCycles){0};
}

HalCore1Test hal_system_core1_test(void)
{
    return (HalCore1Test){.status = HAL_CORE1_UNAVAILABLE};
}

HalCore1Accel hal_system_core1_accel_test(void)
{
    return (HalCore1Accel){.status = HAL_CORE1_UNAVAILABLE};
}

HalStartInfo hal_system_start_info(void)
{
    return (HalStartInfo){.warm = false, .starts = 1};
}

void hal_system_watchdog_test(void)
{
}
