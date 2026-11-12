#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "check.h"
#include "device/image_crc.h"
#include "device/updater.h"

enum {
    IMAGE_SIZE = 0x500004,
    FW_SIZE = 0x44C000,
    FW_END = 0x500000,
    STOCK_SIZE = 39845892,
    STOCK_FW_SIZE = 0x025CC000,
    STOCK_LENGTH = 0x2600000,
    SLOT_1 = 16,
    SLOT_SECTORS = 0x2600000 / 512,
    SLOT_2 = SLOT_1 + SLOT_SECTORS,
    STORAGE_SECTORS = SLOT_2 + SLOT_SECTORS,
    CHUNK_BYTES = 8192,
    PAYLOAD_END = 0x5000,
    CODE_OFFSET = 0xD68,
    CODE_SIZE = PAYLOAD_END - CODE_OFFSET - 8,
    MARKER_LENGTH = 11,
    SLOT_1_HEADER_WRITES = 4
};

#define RESET_HANDLER 0x03050101u

static uint8_t *sectors;
static uint8_t *bytes;
static uint32_t size;
static uint32_t writes_below_slot_1;
static uint32_t last_done;
static uint32_t last_total;
static uint32_t storage_sectors;
static uint32_t write_count;
static uint32_t fence_start;
static uint32_t upper_start;
static uint32_t lower_writes;
static uint32_t upper_writes_after_lower;
static uint32_t last_write_lba;
static uint32_t fail_lower_write;
static uint32_t fail_upper_write;
static uint32_t upper_writes;
static uint32_t slot_1_header_writes;
static bool fail_final_header;
static uint32_t unreadable_from;
static uint8_t *running;

static void put_le32(uint8_t *at, uint32_t value)
{
    at[0] = (uint8_t)value;
    at[1] = (uint8_t)(value >> 8);
    at[2] = (uint8_t)(value >> 16);
    at[3] = (uint8_t)(value >> 24);
}

static uint8_t *own_image(const char *brand)
{
    uint8_t *image = calloc(IMAGE_SIZE, 1);
    for (uint32_t index = 0; index < 0x1F4; index++) {
        image[index] = (uint8_t)(index + 1);
    }
    memset(image + 0x10, 0, 8);
    memcpy(image + 0x10, brand, strlen(brand));
    put_le32(image + 0x1F4, FW_SIZE);
    memcpy(image + 0x1F8, "RKnanoFW", 8);
    put_le32(image + 0x80, PAYLOAD_END);
    for (uint32_t index = CODE_OFFSET; index < PAYLOAD_END; index++) {
        image[index] = (uint8_t)(index * 13 + 1);
    }
    put_le32(image + 0x200, 0x03050000);
    put_le32(image + 0x204, 91);
    uint32_t rows[3][4] = {{0x03050B68, 0x03050000, CODE_SIZE, 0x03050B68 + CODE_SIZE},
                           {0x03004000, 8, 0x03004008, 0x100},
                           {0x03078CD0, 0x03079DCC, 4, 0}};
    for (int row = 0; row < 3; row++) {
        for (int word = 0; word < 4; word++) {
            put_le32(image + 0x208 + 16 * row + 4 * word, rows[row][word]);
        }
    }
    put_le32(image + CODE_OFFSET, 0x03004000);
    put_le32(image + CODE_OFFSET + 4, RESET_HANDLER);
    put_le32(image + CODE_OFFSET + CODE_SIZE, 0x03004000);
    put_le32(image + CODE_OFFSET + CODE_SIZE + 4, RESET_HANDLER);
    memcpy(image + FW_SIZE, image, 512);
    put_le32(image + FW_END, image_crc_update(0, image, FW_END));
    return image;
}

static void seal_stock(uint8_t *image)
{
    memcpy(image + STOCK_FW_SIZE, image, 512);
    put_le32(image + STOCK_LENGTH, image_crc_update(0, image, STOCK_LENGTH));
}

static uint8_t *echo_stock_image(const char *marker, uint32_t marker_at)
{
    uint8_t *image = calloc(STOCK_SIZE, 1);
    for (uint32_t index = 0; index < 0x1F4; index++) {
        image[index] = (uint8_t)(index * 3 + 7);
    }
    memcpy(image + 0x10, "Rockchip", 8);
    put_le32(image + 0x1F4, STOCK_FW_SIZE);
    memcpy(image + 0x1F8, "RKnanoFW", 8);
    for (uint32_t index = 0x200; index < STOCK_FW_SIZE; index += 4093) {
        image[index] = (uint8_t)(index >> 4);
    }
    if (marker != NULL) {
        memcpy(image + marker_at, marker, MARKER_LENGTH);
    }
    seal_stock(image);
    return image;
}

static bool storage_read(void *context, uint32_t lba, uint32_t count, void *buffer)
{
    (void)context;
    if (lba + count > storage_sectors) {
        return false;
    }
    memcpy(buffer, sectors + (size_t)lba * 512, (size_t)count * 512);
    return true;
}

static bool write_is_made_to_fail(uint32_t lba)
{
    bool upper = lba >= upper_start;
    if (upper) {
        upper_writes++;
        upper_writes_after_lower += lower_writes > 0 ? 1 : 0;
        return fail_upper_write != 0 && upper_writes == fail_upper_write;
    }
    lower_writes++;
    slot_1_header_writes += lba == SLOT_1 ? 1 : 0;
    bool final_header = lba == SLOT_1 && slot_1_header_writes == SLOT_1_HEADER_WRITES;
    return (fail_lower_write != 0 && lower_writes == fail_lower_write) ||
           (fail_final_header && final_header);
}

static bool storage_write(void *context, uint32_t lba, uint32_t count, const void *buffer)
{
    (void)context;
    if (lba + count > storage_sectors || lba < fence_start) {
        return false;
    }
    if (write_is_made_to_fail(lba)) {
        return false;
    }
    write_count++;
    last_write_lba = lba;
    writes_below_slot_1 += lba < SLOT_1 ? 1 : 0;
    memcpy(sectors + (size_t)lba * 512, buffer, (size_t)count * 512);
    return true;
}

static bool file_read(void *context, uint32_t offset, void *buffer, uint32_t length)
{
    (void)context;
    if (offset > size || length > size - offset) {
        return false;
    }
    if (unreadable_from != 0 && offset + length > unreadable_from) {
        return false;
    }
    memcpy(buffer, bytes + offset, length);
    return true;
}

static void report(void *context, uint32_t done, uint32_t total)
{
    (void)context;
    last_done = done;
    last_total = total;
}

static void put_running_firmware(void)
{
    uint8_t *slot = sectors + (size_t)SLOT_1 * 512;
    memset(slot, 0x5A, 2 * FW_END);
    put_le32(slot + 0x1F4, FW_SIZE);
    memcpy(slot + 0x1F8, "RKnanoFW", 8);
    memcpy(slot + FW_SIZE, slot, 512);
    memcpy(slot + FW_END, slot, FW_END);
    free(running);
    running = malloc(2 * FW_END);
    memcpy(running, slot, 2 * FW_END);
}

static void load_on(uint8_t *image, uint32_t image_size, uint32_t disk_sectors)
{
    free(sectors);
    free(bytes);
    storage_sectors = disk_sectors;
    sectors = calloc(storage_sectors, 512);
    put_running_firmware();
    bytes = image;
    size = image_size;
    writes_below_slot_1 = 0;
    last_done = 0;
    last_total = 0;
    write_count = 0;
    fence_start = 0;
    upper_start = UINT32_MAX;
    lower_writes = 0;
    upper_writes = 0;
    upper_writes_after_lower = 0;
    last_write_lba = 0;
    fail_lower_write = 0;
    fail_upper_write = 0;
    slot_1_header_writes = 0;
    fail_final_header = false;
    unreadable_from = 0;
}

static void load(uint8_t *image, uint32_t image_size)
{
    load_on(image, image_size, STORAGE_SECTORS);
}

static UpdateCheck check_on(uint8_t *image, uint32_t image_size)
{
    load(image, image_size);
    UpdateStorage storage = {storage_read, storage_write, NULL, SLOT_1, SLOT_2, SLOT_SECTORS};
    UpdateFile file = {file_read, size, NULL};
    return updater_check(&file, &storage);
}

static UpdateCheck check_echo_stock(uint8_t *image)
{
    return check_on(image, STOCK_SIZE);
}

static void a_podium_image_is_valid(void)
{
    CHECK_EQUAL(UPDATE_VALID, check_on(own_image("PODIUM"), IMAGE_SIZE));
}

static void an_echopod_or_recho_image_is_refused(void)
{
    CHECK_EQUAL(UPDATE_INVALID, check_on(own_image("EchoPod"), IMAGE_SIZE));
    CHECK_EQUAL(UPDATE_INVALID, check_on(own_image("RECHO"), IMAGE_SIZE));
}

static void a_damaged_podium_image_is_invalid(void)
{
    uint8_t *image = own_image("PODIUM");
    image[0x2000] ^= 1;
    CHECK_EQUAL(UPDATE_INVALID, check_on(image, IMAGE_SIZE));
}

static void a_stock_echo_image_is_recognised_as_stock(void)
{
    CHECK_EQUAL(UPDATE_STOCK, check_echo_stock(echo_stock_image("ECHOV100IMG", 0x54000)));
}

static void a_stock_echo_marker_across_two_reads_is_found(void)
{
    CHECK_EQUAL(UPDATE_STOCK,
                check_echo_stock(echo_stock_image("ECHOV100IMG", 7 * CHUNK_BYTES - 5)));
    CHECK_EQUAL(UPDATE_STOCK,
                check_echo_stock(echo_stock_image("ECHOV100IMG", 7 * CHUNK_BYTES - 1)));
}

static void a_stock_mini_image_is_refused(void)
{
    uint8_t *image = calloc(0x2000004, 1);
    memcpy(image + 0x10, "Rockchip", 8);
    put_le32(image + 0x1F4, 0x01FCC000);
    memcpy(image + 0x1F8, "RKnanoFW", 8);
    memcpy(image + 0x54000, "HIFIEC00IMG", MARKER_LENGTH);
    memcpy(image + 0x01FCC000, image, 512);
    put_le32(image + 0x2000000, image_crc_update(0, image, 0x2000000));
    CHECK_EQUAL(UPDATE_INVALID, check_on(image, 0x2000004));
}

static void stock_echo_images_without_the_marker_or_with_a_mini_marker_are_invalid(void)
{
    CHECK_EQUAL(UPDATE_INVALID, check_echo_stock(echo_stock_image(NULL, 0)));
    uint8_t *image = echo_stock_image("ECHOV100IMG", 0x54000);
    memcpy(image + 0x80000, "HIFIEC00IMG", MARKER_LENGTH);
    seal_stock(image);
    CHECK_EQUAL(UPDATE_INVALID, check_echo_stock(image));
    image = echo_stock_image("ECHOV100IMG", 0x54000);
    memcpy(image + 0x80000, "MINIV100IMG", MARKER_LENGTH);
    seal_stock(image);
    CHECK_EQUAL(UPDATE_INVALID, check_echo_stock(image));
}

static void stock_echo_images_with_the_wrong_shape_are_invalid(void)
{
    uint8_t *image = echo_stock_image("ECHOV100IMG", 0x54000);
    put_le32(image + 0x1F4, STOCK_FW_SIZE - 0x2000);
    seal_stock(image);
    CHECK_EQUAL(UPDATE_INVALID, check_echo_stock(image));
    image = echo_stock_image("ECHOV100IMG", 0x54000);
    memcpy(image + 0x10, "PODIUM\0\0", 8);
    seal_stock(image);
    CHECK_EQUAL(UPDATE_INVALID, check_echo_stock(image));
    image = echo_stock_image("ECHOV100IMG", 0x54000);
    image[0x100000] ^= 1;
    CHECK_EQUAL(UPDATE_INVALID, check_echo_stock(image));
    image = echo_stock_image("ECHOV100IMG", 0x54000);
    image[STOCK_FW_SIZE + 5] ^= 1;
    CHECK_EQUAL(UPDATE_INVALID, check_echo_stock(image));
    image = echo_stock_image("ECHOV100IMG", 0x54000);
    image[0x1F8] = 'X';
    seal_stock(image);
    CHECK_EQUAL(UPDATE_INVALID, check_echo_stock(image));
    CHECK_EQUAL(UPDATE_INVALID, check_on(echo_stock_image("ECHOV100IMG", 0x54000), STOCK_SIZE - 4));
}

static void a_stock_echo_image_needs_a_slot_it_fits(void)
{
    load(echo_stock_image("ECHOV100IMG", 0x54000), STOCK_SIZE);
    UpdateStorage storage = {storage_read, storage_write, NULL,
                             SLOT_1,       SLOT_2,        STOCK_LENGTH / 512 - 1};
    UpdateFile file = {file_read, size, NULL};
    CHECK_EQUAL(UPDATE_INVALID, updater_check(&file, &storage));
    storage.slot_sectors = STOCK_LENGTH / 512;
    CHECK_EQUAL(UPDATE_STOCK, updater_check(&file, &storage));
}

static void installing_a_stock_echo_image_fills_both_slots_with_every_byte_slot_2_first(void)
{
    uint8_t *image = echo_stock_image("ECHOV100IMG", 0x54000);
    load(image, STOCK_SIZE);
    UpdateStorage storage = {storage_read, storage_write, NULL, SLOT_1, SLOT_2, SLOT_SECTORS};
    UpdateFile file = {file_read, size, NULL};
    CHECK(updater_install(&file, &storage, (UpdateProgress){report, NULL}));
    CHECK(memcmp(sectors + (size_t)SLOT_2 * 512, image, STOCK_LENGTH) == 0);
    CHECK(memcmp(sectors + (size_t)SLOT_1 * 512, image, STOCK_LENGTH) == 0);
    CHECK_EQUAL(0, writes_below_slot_1);
    CHECK_EQUAL(last_total, last_done);
    CHECK_EQUAL(2 * (STOCK_LENGTH / CHUNK_BYTES), last_total);
}

static void a_stock_echo_image_is_installed_whole_when_slot_2_sits_5_mib_after_slot_1(void)
{
    uint8_t *image = echo_stock_image("ECHOV100IMG", 0x54000);
    load(image, STOCK_SIZE);
    UpdateStorage storage = {storage_read, storage_write,         NULL,
                             SLOT_1,       SLOT_1 + FW_END / 512, SLOT_SECTORS};
    UpdateFile file = {file_read, size, NULL};
    CHECK(updater_install(&file, &storage, (UpdateProgress){report, NULL}));
    CHECK(memcmp(sectors + (size_t)SLOT_1 * 512, image, STOCK_LENGTH) == 0);
    CHECK(memcmp(sectors + (size_t)(SLOT_1 + SLOT_SECTORS) * 512, image, STOCK_LENGTH) == 0);
    CHECK_EQUAL(0, writes_below_slot_1);
}

static uint8_t *dense_stock_image(void)
{
    uint8_t *image = echo_stock_image(NULL, 0);
    uint32_t state = 0x9E3779B9u;
    for (uint32_t index = 0x200; index < STOCK_LENGTH; index++) {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        image[index] = (uint8_t)(state >> 11);
    }
    memcpy(image + 0x54000, "ECHOV100IMG", MARKER_LENGTH);
    seal_stock(image);
    return image;
}

static UpdateStorage echo_storage(uint32_t area_mib, uint32_t slot_2_offset)
{
    uint32_t half = area_mib * 2048 / 2;
    upper_start = SLOT_1 + half;
    fence_start = SLOT_1;
    return (UpdateStorage){storage_read, storage_write, NULL, SLOT_1, SLOT_1 + slot_2_offset, half};
}

static uint8_t *load_dense_stock(uint32_t area_mib)
{
    uint8_t *image = dense_stock_image();
    load_on(image, STOCK_SIZE, SLOT_1 + area_mib * 2048);
    return image;
}

static bool copy_is_the_whole_image(uint32_t lba, const uint8_t *image)
{
    const uint8_t *copy = sectors + (size_t)lba * 512;
    uint32_t crc = (uint32_t)image[STOCK_LENGTH] | (uint32_t)image[STOCK_LENGTH + 1] << 8 |
                   (uint32_t)image[STOCK_LENGTH + 2] << 16 |
                   (uint32_t)image[STOCK_LENGTH + 3] << 24;
    return memcmp(copy, image, STOCK_LENGTH) == 0 && memcmp(copy + 0x1F8, "RKnanoFW", 8) == 0 &&
           memcmp(copy + STOCK_FW_SIZE, copy, 512) == 0 &&
           image_crc_update(0, copy, STOCK_LENGTH) == crc;
}

static bool slot_1_header_is_invalid(void)
{
    return memcmp(sectors + (size_t)SLOT_1 * 512 + 0x1F8, "RKnanoFW", 8) != 0;
}

static void restore_and_check_every_byte(uint32_t area_mib, uint32_t slot_2_offset)
{
    uint8_t *image = load_dense_stock(area_mib);
    uint8_t *pristine = malloc(STOCK_SIZE);
    memcpy(pristine, image, STOCK_SIZE);
    UpdateStorage storage = echo_storage(area_mib, slot_2_offset);
    UpdateFile file = {file_read, size, NULL};
    CHECK_EQUAL(UPDATE_STOCK, updater_check(&file, &storage));
    CHECK_EQUAL(0u, write_count);
    CHECK(updater_install(&file, &storage, (UpdateProgress){report, NULL}));
    CHECK(copy_is_the_whole_image(SLOT_1, image));
    CHECK(copy_is_the_whole_image(upper_start, image));
    CHECK(memcmp(image, pristine, STOCK_SIZE) == 0);
    CHECK_EQUAL(0u, upper_writes_after_lower);
    CHECK_EQUAL((uint32_t)SLOT_1, last_write_lba);
    CHECK_EQUAL(last_total, last_done);
    free(pristine);
}

static void a_stock_restore_is_byte_identical_on_every_area_and_slot_2_position(void)
{
    static const uint32_t areas_mib[] = {76, 80, 96, 128};
    for (size_t area = 0; area < sizeof areas_mib / sizeof areas_mib[0]; area++) {
        uint32_t half = areas_mib[area] * 2048 / 2;
        uint32_t offsets[] = {16, 0x2800, STOCK_LENGTH / 512 - 16, half - 16, half};
        for (size_t offset = 0; offset < sizeof offsets / sizeof offsets[0]; offset++) {
            restore_and_check_every_byte(areas_mib[area], offsets[offset]);
        }
    }
}

static void a_stock_restore_into_an_area_one_chunk_too_small_writes_nothing(void)
{
    load_dense_stock(76);
    UpdateStorage storage = echo_storage(76, 0x2800);
    storage.slot_sectors -= 16;
    UpdateFile file = {file_read, size, NULL};
    CHECK_EQUAL(UPDATE_INVALID, updater_check(&file, &storage));
    CHECK(!updater_install(&file, &storage, (UpdateProgress){report, NULL}));
    CHECK_EQUAL(0u, write_count);
    load_dense_stock(74);
    storage = echo_storage(74, 0x2800);
    CHECK_EQUAL(UPDATE_INVALID, updater_check(&file, &storage));
    CHECK(!updater_install(&file, &storage, (UpdateProgress){report, NULL}));
    CHECK_EQUAL(0u, write_count);
}

static bool running_firmware_is_untouched(void)
{
    return memcmp(sectors + (size_t)SLOT_1 * 512, running, 2 * FW_END) == 0;
}

static void a_failure_while_the_second_copy_is_written_leaves_the_running_firmware_alone(void)
{
    load_dense_stock(76);
    UpdateStorage storage = echo_storage(76, 0x2800);
    fail_upper_write = 2000;
    UpdateFile file = {file_read, size, NULL};
    CHECK(!updater_install(&file, &storage, (UpdateProgress){report, NULL}));
    CHECK_EQUAL(1999u, write_count);
    CHECK_EQUAL(0u, lower_writes);
    CHECK(running_firmware_is_untouched());
    CHECK(last_done < last_total);
}

static void a_failure_while_slot_1_is_rewritten_keeps_the_whole_second_copy(void)
{
    uint8_t *image = load_dense_stock(76);
    UpdateStorage storage = echo_storage(76, 0x2800);
    fail_lower_write = 1500;
    UpdateFile file = {file_read, size, NULL};
    CHECK(!updater_install(&file, &storage, (UpdateProgress){report, NULL}));
    CHECK(copy_is_the_whole_image(upper_start, image));
    CHECK(slot_1_header_is_invalid());
    CHECK(last_done < last_total);
}

static void a_failure_on_the_last_header_write_is_not_reported_as_success(void)
{
    uint8_t *image = load_dense_stock(76);
    UpdateStorage storage = echo_storage(76, 0x2800);
    fail_final_header = true;
    UpdateFile file = {file_read, size, NULL};
    CHECK(!updater_install(&file, &storage, (UpdateProgress){report, NULL}));
    CHECK(copy_is_the_whole_image(upper_start, image));
    CHECK(slot_1_header_is_invalid());
    const uint8_t *slot_1 = sectors + (size_t)SLOT_1 * 512;
    CHECK(memcmp(slot_1, image, 0x1F8) == 0);
    CHECK(memcmp(slot_1 + 0x200, image + 0x200, STOCK_LENGTH - 0x200) == 0);
    CHECK(last_done < last_total);
}

static void a_stock_file_that_is_cut_short_or_stops_reading_is_refused_or_fails(void)
{
    CHECK_EQUAL(UPDATE_INVALID, check_on(dense_stock_image(), STOCK_SIZE - CHUNK_BYTES));
    CHECK_EQUAL(UPDATE_INVALID, check_on(dense_stock_image(), STOCK_SIZE + CHUNK_BYTES));
    load_dense_stock(76);
    UpdateStorage storage = echo_storage(76, 0x2800);
    unreadable_from = STOCK_LENGTH / 2;
    UpdateFile file = {file_read, size, NULL};
    CHECK_EQUAL(UPDATE_INVALID, updater_check(&file, &storage));
    CHECK_EQUAL(0u, write_count);
    CHECK(!updater_install(&file, &storage, (UpdateProgress){report, NULL}));
    CHECK(write_count > 0);
    CHECK_EQUAL(0u, lower_writes);
    CHECK(running_firmware_is_untouched());
}

static void put_loader_valid_copy(uint32_t lba)
{
    uint8_t *copy = sectors + (size_t)lba * 512;
    memset(copy, 0x33, 1024);
    put_le32(copy + 0x1F4, 512);
    memcpy(copy + 0x1F8, "RKnanoFW", 8);
    memcpy(copy + 512, copy, 512);
}

static bool install_podium_with_slot_2_at_5_mib(uint8_t *image)
{
    UpdateStorage storage = echo_storage(76, 0x2800);
    UpdateFile file = {file_read, size, NULL};
    bool installed = updater_install(&file, &storage, (UpdateProgress){report, NULL});
    bool second_copy_written =
        memcmp(sectors + (size_t)(SLOT_1 + 0x2800) * 512, image, FW_END) == 0;
    CHECK(second_copy_written);
    return installed;
}

static void an_update_stops_before_slot_1_when_the_loader_would_find_no_other_copy(void)
{
    uint8_t *image = own_image("PODIUM");
    load_on(image, IMAGE_SIZE, SLOT_1 + 76 * 2048);
    memset(sectors + (size_t)(SLOT_1 + 0x2800) * 512, 0, FW_END);
    uint8_t *slot_1_before = malloc(FW_END);
    memcpy(slot_1_before, sectors + (size_t)SLOT_1 * 512, FW_END);
    CHECK(!install_podium_with_slot_2_at_5_mib(image));
    CHECK(memcmp(sectors + (size_t)SLOT_1 * 512, slot_1_before, FW_END) == 0);
    uint32_t half = 76 * 2048 / 2;
    put_loader_valid_copy(SLOT_1 + half - 16 * 2048);
    CHECK(!install_podium_with_slot_2_at_5_mib(image));
    CHECK(memcmp(sectors + (size_t)SLOT_1 * 512, slot_1_before, FW_END) == 0);
    put_loader_valid_copy(SLOT_1 + half - 3 * 2048);
    sectors[(size_t)(SLOT_1 + half - 3 * 2048 + 1) * 512 + 9] ^= 1;
    CHECK(!install_podium_with_slot_2_at_5_mib(image));
    CHECK(memcmp(sectors + (size_t)SLOT_1 * 512, slot_1_before, FW_END) == 0);
    free(slot_1_before);
}

static void an_update_goes_ahead_when_a_whole_copy_waits_where_the_loader_looks(void)
{
    static const uint32_t mib_below_half[] = {0, 3, 15};
    for (size_t index = 0; index < sizeof mib_below_half / sizeof mib_below_half[0]; index++) {
        uint8_t *image = own_image("PODIUM");
        load_on(image, IMAGE_SIZE, SLOT_1 + 76 * 2048);
        uint32_t fallback = SLOT_1 + 76 * 2048 / 2 - mib_below_half[index] * 2048;
        put_loader_valid_copy(fallback);
        uint8_t kept[1024];
        memcpy(kept, sectors + (size_t)fallback * 512, sizeof kept);
        CHECK(install_podium_with_slot_2_at_5_mib(image));
        CHECK(memcmp(sectors + (size_t)SLOT_1 * 512, image, FW_END) == 0);
        CHECK(memcmp(sectors + (size_t)fallback * 512, kept, sizeof kept) == 0);
    }
}

static void installing_a_podium_image_fills_both_slots(void)
{
    uint8_t *image = own_image("PODIUM");
    load(image, IMAGE_SIZE);
    UpdateStorage storage = {storage_read, storage_write, NULL, SLOT_1, SLOT_2, SLOT_SECTORS};
    UpdateFile file = {file_read, size, NULL};
    CHECK(updater_install(&file, &storage, (UpdateProgress){report, NULL}));
    CHECK(memcmp(sectors + (size_t)SLOT_2 * 512, image, FW_END) == 0);
    CHECK(memcmp(sectors + (size_t)SLOT_1 * 512, image, FW_END) == 0);
    CHECK_EQUAL(UPDATE_ALREADY_INSTALLED, updater_check(&file, &storage));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(a_podium_image_is_valid),
        TEST_CASE(an_echopod_or_recho_image_is_refused),
        TEST_CASE(a_damaged_podium_image_is_invalid),
        TEST_CASE(a_stock_echo_image_is_recognised_as_stock),
        TEST_CASE(a_stock_echo_marker_across_two_reads_is_found),
        TEST_CASE(a_stock_mini_image_is_refused),
        TEST_CASE(stock_echo_images_without_the_marker_or_with_a_mini_marker_are_invalid),
        TEST_CASE(stock_echo_images_with_the_wrong_shape_are_invalid),
        TEST_CASE(a_stock_echo_image_needs_a_slot_it_fits),
        TEST_CASE(installing_a_stock_echo_image_fills_both_slots_with_every_byte_slot_2_first),
        TEST_CASE(a_stock_echo_image_is_installed_whole_when_slot_2_sits_5_mib_after_slot_1),
        TEST_CASE(a_stock_restore_is_byte_identical_on_every_area_and_slot_2_position),
        TEST_CASE(a_stock_restore_into_an_area_one_chunk_too_small_writes_nothing),
        TEST_CASE(a_failure_while_the_second_copy_is_written_leaves_the_running_firmware_alone),
        TEST_CASE(a_failure_while_slot_1_is_rewritten_keeps_the_whole_second_copy),
        TEST_CASE(a_failure_on_the_last_header_write_is_not_reported_as_success),
        TEST_CASE(a_stock_file_that_is_cut_short_or_stops_reading_is_refused_or_fails),
        TEST_CASE(installing_a_podium_image_fills_both_slots),
        TEST_CASE(an_update_stops_before_slot_1_when_the_loader_would_find_no_other_copy),
        TEST_CASE(an_update_goes_ahead_when_a_whole_copy_waits_where_the_loader_looks),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
