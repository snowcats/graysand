#include <string.h>

#include "check.h"
#include "device/emmc_layout.h"
#include "fake_disk.h"

enum {
    TOTAL_SECTORS = 15269888,
    RECHORD_SYSTEM_MIB = 0x44
};

static void write_idb(FakeDisk *disk, int copy, uint16_t system_mib)
{
    uint32_t lba = 0x40u + (uint32_t)copy * 0x400u;
    uint8_t *first = fake_disk_sector(disk, lba);
    uint8_t *second = fake_disk_sector(disk, lba + 1);
    first[0] = 0x3B;
    first[1] = 0x8C;
    first[2] = 0xDC;
    first[3] = 0xFC;
    second[0x02] = (uint8_t)system_mib;
    second[0x03] = (uint8_t)(system_mib >> 8);
    memcpy(second + 0x0A, "NANO", 4);
}

static void write_fat_boot_sector(FakeDisk *disk, uint32_t lba)
{
    uint8_t *sector = fake_disk_sector(disk, lba);
    sector[0] = 0xEB;
    sector[11] = 0x00;
    sector[12] = 0x02;
    sector[510] = 0x55;
    sector[511] = 0xAA;
}

static void write_save_block(FakeDisk *disk, uint32_t lba)
{
    uint8_t *sector = fake_disk_sector(disk, lba);
    memcpy(sector, "SAVE", 4);
}

static bool read_layout(FakeDisk *disk, EmmcLayout *layout)
{
    BlockDevice device = fake_disk_device(disk);
    return emmc_layout_read(layout, &device, TOTAL_SECTORS);
}

static void the_firmware_area_and_slots_follow_the_idb_system_size(void)
{
    static FakeDisk disk;
    fake_disk_init(&disk, TOTAL_SECTORS);
    write_idb(&disk, 0, RECHORD_SYSTEM_MIB);
    write_fat_boot_sector(&disk, 0x24000);
    EmmcLayout layout;
    CHECK(read_layout(&disk, &layout));
    CHECK_EQUAL(0x2000, layout.slot_1);
    CHECK_EQUAL(0x13000, layout.slot_2);
    CHECK_EQUAL(0x11000, layout.slot_sectors);
    CHECK_EQUAL(0x24000, layout.firmware_end);
}

static void the_volume_right_after_the_firmware_is_found(void)
{
    static FakeDisk disk;
    fake_disk_init(&disk, TOTAL_SECTORS);
    write_idb(&disk, 0, RECHORD_SYSTEM_MIB);
    write_fat_boot_sector(&disk, 0x24000);
    EmmcLayout layout;
    CHECK(read_layout(&disk, &layout));
    CHECK_EQUAL(0x24000, layout.volume_start);
    CHECK_EQUAL(TOTAL_SECTORS - 0x24000, layout.volume_sectors);
}

static void a_volume_moved_past_a_save_block_is_found_by_scanning(void)
{
    static FakeDisk disk;
    fake_disk_init(&disk, TOTAL_SECTORS);
    write_idb(&disk, 0, RECHORD_SYSTEM_MIB);
    write_save_block(&disk, 0x24000);
    write_fat_boot_sector(&disk, 0x88000);
    EmmcLayout layout;
    CHECK(read_layout(&disk, &layout));
    CHECK_EQUAL(0x88000, layout.volume_start);
    CHECK_EQUAL(TOTAL_SECTORS - 0x88000, layout.volume_sectors);
}

static void an_mbr_or_exfat_volume_is_also_accepted(void)
{
    static FakeDisk disk;
    fake_disk_init(&disk, TOTAL_SECTORS);
    write_idb(&disk, 0, RECHORD_SYSTEM_MIB);
    uint8_t *mbr = fake_disk_sector(&disk, 0x24800);
    mbr[0x1BE] = 0x00;
    mbr[0x1C2] = 0x0C;
    mbr[0x1C6] = 0x3F;
    mbr[510] = 0x55;
    mbr[511] = 0xAA;
    EmmcLayout layout;
    CHECK(read_layout(&disk, &layout));
    CHECK_EQUAL(0x24800, layout.volume_start);

    fake_disk_init(&disk, TOTAL_SECTORS);
    write_idb(&disk, 0, RECHORD_SYSTEM_MIB);
    uint8_t *exfat = fake_disk_sector(&disk, 0x25000);
    memcpy(exfat + 3, "EXFAT   ", 8);
    exfat[510] = 0x55;
    exfat[511] = 0xAA;
    CHECK(read_layout(&disk, &layout));
    CHECK_EQUAL(0x25000, layout.volume_start);
}

static void the_first_valid_idb_copy_is_used(void)
{
    static FakeDisk disk;
    fake_disk_init(&disk, TOTAL_SECTORS);
    write_idb(&disk, 2, 0x20);
    write_idb(&disk, 3, RECHORD_SYSTEM_MIB);
    write_fat_boot_sector(&disk, 0x2000u + 0x20u * 0x800u);
    EmmcLayout layout;
    CHECK(read_layout(&disk, &layout));
    CHECK_EQUAL(0x2000u + 0x20u * 0x800u, layout.firmware_end);
}

static void an_idb_without_the_nano_tag_is_ignored(void)
{
    static FakeDisk disk;
    fake_disk_init(&disk, TOTAL_SECTORS);
    write_idb(&disk, 0, RECHORD_SYSTEM_MIB);
    memcpy(fake_disk_sector(&disk, 0x41) + 0x0A, "NONO", 4);
    write_fat_boot_sector(&disk, 0x24000);
    EmmcLayout layout;
    CHECK(!read_layout(&disk, &layout));
}

static void no_volume_within_the_scan_limit_fails(void)
{
    static FakeDisk disk;
    fake_disk_init(&disk, TOTAL_SECTORS);
    write_idb(&disk, 0, RECHORD_SYSTEM_MIB);
    write_fat_boot_sector(&disk, 0x2000u + 0x100000u);
    EmmcLayout layout;
    CHECK(!read_layout(&disk, &layout));
}

static void a_volume_is_never_accepted_inside_the_firmware_area(void)
{
    static FakeDisk disk;
    fake_disk_init(&disk, TOTAL_SECTORS);
    write_idb(&disk, 0, RECHORD_SYSTEM_MIB);
    write_fat_boot_sector(&disk, 0x13000);
    write_fat_boot_sector(&disk, 0x30000);
    EmmcLayout layout;
    CHECK(read_layout(&disk, &layout));
    CHECK_EQUAL(0x30000, layout.volume_start);
}

static EmmcLayout rechord_layout(void)
{
    return (EmmcLayout){.slot_1 = 0x2000,
                        .slot_2 = 0x13000,
                        .slot_sectors = 0x11000,
                        .firmware_end = 0x24000,
                        .volume_start = 0x88000,
                        .volume_sectors = TOTAL_SECTORS - 0x88000};
}

static void writes_inside_the_user_volume_are_allowed(void)
{
    EmmcLayout layout = rechord_layout();
    CHECK(emmc_layout_allows_write(&layout, 0x88000, 8));
    CHECK(emmc_layout_allows_write(&layout, TOTAL_SECTORS - 1, 1));
    CHECK(!emmc_layout_allows_write(&layout, TOTAL_SECTORS - 1, 2));
}

static void the_volume_window_is_relative_to_the_volume_and_bounded(void)
{
    EmmcLayout layout = rechord_layout();
    CHECK(emmc_layout_in_volume(&layout, 0, 8));
    CHECK(emmc_layout_in_volume(&layout, layout.volume_sectors - 1, 1));
    CHECK(!emmc_layout_in_volume(&layout, layout.volume_sectors - 1, 2));
    CHECK(!emmc_layout_in_volume(&layout, layout.volume_sectors, 1));
    CHECK(!emmc_layout_in_volume(&layout, 0xFFFFFFFFu, 2));
}

static void writes_inside_one_firmware_slot_are_allowed(void)
{
    EmmcLayout layout = rechord_layout();
    CHECK(emmc_layout_allows_write(&layout, 0x2000, 16));
    CHECK(emmc_layout_allows_write(&layout, 0x13000 + 0x11000 - 1, 1));
    CHECK(!emmc_layout_allows_write(&layout, 0x13000 - 1, 2));
}

static bool stays_inside(uint64_t start, uint64_t length, uint32_t lba, uint32_t count)
{
    return lba >= start && (uint64_t)lba + count <= start + length;
}

static uint32_t check_fence_around(const EmmcLayout *layout, uint32_t edge)
{
    static const uint32_t counts[] = {1, 2, 15, 16, 17, 0x800, 0x13000, 0x7FFFFFFFu, 0xFFFFFFFFu};
    uint32_t escapes = 0;
    for (int32_t step = -17; step <= 17; step++) {
        uint32_t lba = edge + (uint32_t)step;
        for (size_t index = 0; index < sizeof counts / sizeof counts[0]; index++) {
            uint32_t count = counts[index];
            bool in_firmware =
                stays_inside(layout->slot_1, 2ull * layout->slot_sectors, lba, count);
            bool in_volume = stays_inside(layout->volume_start, layout->volume_sectors, lba, count);
            if (emmc_layout_allows_write(layout, lba, count) && !in_firmware && !in_volume) {
                escapes++;
            }
        }
    }
    return escapes;
}

static void no_allowed_write_ever_leaves_the_firmware_area_or_the_volume(void)
{
    static const uint32_t areas_mib[] = {1, 2, 68, 74, 76, 96, 128, 1024, 0xFFFF};
    uint32_t escapes = 0;
    uint32_t layouts = 0;
    for (size_t area = 0; area < sizeof areas_mib / sizeof areas_mib[0]; area++) {
        uint32_t half = areas_mib[area] * 0x800u / 2;
        uint32_t slot_2_offsets[] = {0x800, 0x2800, half / 2, half - 0x800, half};
        for (size_t slot = 0; slot < sizeof slot_2_offsets / sizeof slot_2_offsets[0]; slot++) {
            if (slot_2_offsets[slot] == 0 || slot_2_offsets[slot] > half) {
                continue;
            }
            for (uint32_t gap = 0; gap <= 0x800; gap += 0x800) {
                EmmcLayout layout = {.slot_1 = 0x2000,
                                     .slot_2 = 0x2000 + slot_2_offsets[slot],
                                     .slot_sectors = half,
                                     .firmware_end = 0x2000 + 2 * half,
                                     .volume_start = 0x2000 + 2 * half + gap,
                                     .volume_sectors = 0x100000};
                uint32_t edges[] = {0,
                                    0x40,
                                    layout.slot_1,
                                    layout.slot_2,
                                    layout.slot_1 + half,
                                    layout.slot_2 + half,
                                    layout.firmware_end,
                                    layout.volume_start,
                                    layout.volume_start + layout.volume_sectors,
                                    0xFFFFFFF0u};
                for (size_t edge = 0; edge < sizeof edges / sizeof edges[0]; edge++) {
                    escapes += check_fence_around(&layout, edges[edge]);
                }
                layouts++;
            }
        }
    }
    CHECK(layouts > 60);
    CHECK_EQUAL(0u, escapes);
}

static void writes_to_the_idb_the_save_block_or_the_gap_are_refused(void)
{
    EmmcLayout layout = rechord_layout();
    CHECK(!emmc_layout_allows_write(&layout, 0x40, 1));
    CHECK(!emmc_layout_allows_write(&layout, 0x1FFF, 1));
    CHECK(!emmc_layout_allows_write(&layout, 0x24000, 1));
    CHECK(!emmc_layout_allows_write(&layout, 0x87FFF, 2));
    CHECK(!emmc_layout_allows_write(&layout, 0x88000, 0));
}

static void write_firmware_header(FakeDisk *disk, uint32_t lba, uint32_t fw_size, bool with_copy)
{
    uint8_t *header = fake_disk_sector(disk, lba);
    memcpy(header + 0x1F8, "RKnanoFW", 8);
    header[0x1F4] = (uint8_t)fw_size;
    header[0x1F5] = (uint8_t)(fw_size >> 8);
    header[0x1F6] = (uint8_t)(fw_size >> 16);
    header[0x1F7] = (uint8_t)(fw_size >> 24);
    header[0] = (uint8_t)(lba >> 3) | 1;
    if (with_copy) {
        memcpy(fake_disk_sector(disk, lba + fw_size / 512), header, 512);
    }
}

static bool read_searching(FakeDisk *disk, EmmcLayout *layout)
{
    BlockDevice device = fake_disk_device(disk);
    return emmc_layout_read_with(layout, &device, TOTAL_SECTORS, EMMC_SLOT_2_SEARCH);
}

static void prepare_echo(FakeDisk *disk, uint32_t slot_1_fw_size)
{
    fake_disk_init(disk, TOTAL_SECTORS);
    write_idb(disk, 0, 0x4C);
    write_firmware_header(disk, 0x2000, slot_1_fw_size, true);
    write_fat_boot_sector(disk, 0x28000);
}

static void the_search_finds_slot_2_where_stock_puts_it_on_the_echo_layout(void)
{
    static FakeDisk disk;
    prepare_echo(&disk, 0x025CC000);
    write_firmware_header(&disk, 0x2000 + 0x13000, 0x025CC000, true);
    EmmcLayout layout;
    CHECK(read_searching(&disk, &layout));
    CHECK_EQUAL(0x2000 + 0x13000, layout.slot_2);
    CHECK_EQUAL(0x13000, layout.slot_sectors);
    CHECK_EQUAL(0x28000, layout.firmware_end);
}

static void the_search_starts_at_the_next_mib_after_the_firmware_plus_16_kib(void)
{
    static FakeDisk disk;
    prepare_echo(&disk, 0x01FCC000);
    write_firmware_header(&disk, 0x2000 + 0x10800, 0x01FCC000, true);
    EmmcLayout layout;
    CHECK(read_searching(&disk, &layout));
    CHECK_EQUAL(0x2000 + 0x10800, layout.slot_2);
}

static void the_search_takes_the_first_header_it_meets(void)
{
    static FakeDisk disk;
    prepare_echo(&disk, 0x01FCC000);
    write_firmware_header(&disk, 0x2000 + 0x10000, 0x01FCC000, true);
    write_firmware_header(&disk, 0x2000 + 0x11000, 0x01FCC000, true);
    EmmcLayout layout;
    CHECK(read_searching(&disk, &layout));
    CHECK_EQUAL(0x2000 + 0x10000, layout.slot_2);
}

static void without_a_header_the_search_falls_back_to_half_the_firmware_area(void)
{
    static FakeDisk disk;
    prepare_echo(&disk, 0x025CC000);
    EmmcLayout layout;
    CHECK(read_searching(&disk, &layout));
    CHECK_EQUAL(0x2000 + 0x13000, layout.slot_2);
}

static void a_podium_sized_slot_1_searches_below_slot_2_and_falls_back_to_half(void)
{
    static FakeDisk disk;
    prepare_echo(&disk, 0x44C000);
    write_firmware_header(&disk, 0x2000 + 0x13000, 0x44C000, true);
    EmmcLayout layout;
    CHECK(read_searching(&disk, &layout));
    CHECK_EQUAL(0x2000 + 0x13000, layout.slot_2);
}

static void a_header_whose_copy_is_missing_is_not_used(void)
{
    static FakeDisk disk;
    prepare_echo(&disk, 0x01FCC000);
    write_firmware_header(&disk, 0x2000 + 0x10800, 0x01FCC000, false);
    EmmcLayout layout;
    CHECK(read_searching(&disk, &layout));
    CHECK_EQUAL(0x2000 + 0x13000, layout.slot_2);
}

static void a_header_past_half_the_firmware_area_is_not_used(void)
{
    static FakeDisk disk;
    prepare_echo(&disk, 0x025CC000);
    write_firmware_header(&disk, 0x2000 + 0x13800, 0x025CC000, true);
    EmmcLayout layout;
    CHECK(read_searching(&disk, &layout));
    CHECK_EQUAL(0x2000 + 0x13000, layout.slot_2);
}

static void an_invalid_slot_1_falls_back_to_half_the_firmware_area(void)
{
    static FakeDisk disk;
    prepare_echo(&disk, 0x025CC000);
    memset(fake_disk_sector(&disk, 0x2000), 0, 512);
    write_firmware_header(&disk, 0x2000 + 0x13000, 0x025CC000, true);
    EmmcLayout layout;
    CHECK(read_searching(&disk, &layout));
    CHECK_EQUAL(0x2000 + 0x13000, layout.slot_2);
}

static void the_search_keeps_the_slot_length_and_the_write_fence(void)
{
    static FakeDisk disk;
    prepare_echo(&disk, 0x01FCC000);
    write_firmware_header(&disk, 0x2000 + 0x10800, 0x01FCC000, true);
    EmmcLayout layout;
    CHECK(read_searching(&disk, &layout));
    CHECK_EQUAL(0x13000, layout.slot_sectors);
    CHECK(emmc_layout_allows_write(&layout, layout.slot_2, 16));
    CHECK(!emmc_layout_allows_write(&layout, layout.firmware_end - 1, 2));
    CHECK(layout.slot_2 + layout.slot_sectors <= layout.firmware_end);
}

static void a_searched_slot_2_still_leaves_the_upper_half_writable_for_a_stock_restore(void)
{
    static FakeDisk disk;
    prepare_echo(&disk, 0x44C000);
    write_firmware_header(&disk, 0x2000 + 0x2800, 0x44C000, true);
    EmmcLayout layout;
    CHECK(read_searching(&disk, &layout));
    CHECK_EQUAL(0x2000 + 0x2800, layout.slot_2);
    CHECK(emmc_layout_allows_write(&layout, layout.firmware_end - 16, 16));
    CHECK(!emmc_layout_allows_write(&layout, layout.firmware_end - 1, 2));
}

static void the_default_read_never_searches(void)
{
    static FakeDisk disk;
    prepare_echo(&disk, 0x01FCC000);
    write_firmware_header(&disk, 0x2000 + 0x10800, 0x01FCC000, true);
    EmmcLayout layout;
    CHECK(read_layout(&disk, &layout));
    CHECK_EQUAL(0x2000 + 0x13000, layout.slot_2);
}

static void a_corrupt_system_size_is_rejected(void)
{
    static FakeDisk disk;
    fake_disk_init(&disk, TOTAL_SECTORS);
    write_idb(&disk, 0, 0xFFFF);
    write_fat_boot_sector(&disk, 0x24000);
    EmmcLayout layout;
    CHECK(!read_layout(&disk, &layout));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(the_firmware_area_and_slots_follow_the_idb_system_size),
        TEST_CASE(the_volume_right_after_the_firmware_is_found),
        TEST_CASE(a_volume_moved_past_a_save_block_is_found_by_scanning),
        TEST_CASE(an_mbr_or_exfat_volume_is_also_accepted),
        TEST_CASE(the_first_valid_idb_copy_is_used),
        TEST_CASE(an_idb_without_the_nano_tag_is_ignored),
        TEST_CASE(a_corrupt_system_size_is_rejected),
        TEST_CASE(the_search_finds_slot_2_where_stock_puts_it_on_the_echo_layout),
        TEST_CASE(the_search_starts_at_the_next_mib_after_the_firmware_plus_16_kib),
        TEST_CASE(the_search_takes_the_first_header_it_meets),
        TEST_CASE(without_a_header_the_search_falls_back_to_half_the_firmware_area),
        TEST_CASE(a_podium_sized_slot_1_searches_below_slot_2_and_falls_back_to_half),
        TEST_CASE(a_header_whose_copy_is_missing_is_not_used),
        TEST_CASE(a_header_past_half_the_firmware_area_is_not_used),
        TEST_CASE(an_invalid_slot_1_falls_back_to_half_the_firmware_area),
        TEST_CASE(the_search_keeps_the_slot_length_and_the_write_fence),
        TEST_CASE(a_searched_slot_2_still_leaves_the_upper_half_writable_for_a_stock_restore),
        TEST_CASE(the_default_read_never_searches),
        TEST_CASE(no_volume_within_the_scan_limit_fails),
        TEST_CASE(a_volume_is_never_accepted_inside_the_firmware_area),
        TEST_CASE(writes_inside_the_user_volume_are_allowed),
        TEST_CASE(the_volume_window_is_relative_to_the_volume_and_bounded),
        TEST_CASE(writes_inside_one_firmware_slot_are_allowed),
        TEST_CASE(writes_to_the_idb_the_save_block_or_the_gap_are_refused),
        TEST_CASE(no_allowed_write_ever_leaves_the_firmware_area_or_the_volume),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
