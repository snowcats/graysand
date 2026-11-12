#include <stdbool.h>
#include <stddef.h>

#include "check.h"
#include "device/fat.h"
#include "device/fat_space.h"
#include "sparse_card.h"

enum {
    SECTOR_SIZE = 512,
    BUFFER_SECTORS = 8,
    DELETED_BYTES = 20000
};

static const char DELETED_PATH[] = "/Music/Sigur R\xc3\xb3s/\xc3\x81g\xc3\xa6tis byrjun/"
                                   "02 Svefn-g-englar with a long name.flac";

static uint8_t buffer[BUFFER_SECTORS * SECTOR_SIZE];

typedef struct Card {
    SparseCard sparse;
    FatVolume volume;
} Card;

static Card *mounted(const char *name)
{
    static Card card;
    CHECK(sparse_card_load(&card.sparse, name));
    CHECK(fat_mount(&card.volume, sparse_card_device(&card.sparse)));
    return &card;
}

static uint32_t free_of(Card *card, uint32_t sectors)
{
    uint32_t count = 0;
    CHECK(fat_free_clusters(&card->volume, buffer, sectors, &count));
    return count;
}

static void check_card(const char *name)
{
    Card *card = mounted(name);
    uint32_t before = free_of(card, BUFFER_SECTORS);
    CHECK(before > 0 && before < card->volume.cluster_count);
    CHECK_EQUAL(before, free_of(card, 1));
    uint32_t cluster_bytes = (uint32_t)SECTOR_SIZE << card->volume.cluster_shift;
    CHECK(fat_delete(&card->volume, DELETED_PATH));
    CHECK_EQUAL(before + (DELETED_BYTES + cluster_bytes - 1) / cluster_bytes,
                free_of(card, BUFFER_SECTORS));
    sparse_card_free(&card->sparse);
}

static void fat32_free_space_follows_a_delete(void)
{
    check_card("fat32_mbr");
}

static void fat16_free_space_follows_a_delete(void)
{
    check_card("fat16_superfloppy");
}

static void exfat_free_space_follows_a_delete(void)
{
    check_card("exfat_mbr");
}

static void fat32_without_a_free_count_is_not_guessed(void)
{
    Card *card = mounted("fat32_mbr");
    uint8_t *free_count = card->sparse.bytes + (size_t)card->volume.info_sector * SECTOR_SIZE + 488;
    free_count[0] = free_count[1] = free_count[2] = free_count[3] = 0xFF;
    uint32_t count = 0;
    CHECK(!fat_free_clusters(&card->volume, buffer, BUFFER_SECTORS, &count));
    sparse_card_free(&card->sparse);
}

static void clusters_turn_into_megabytes(void)
{
    FatVolume volume = {.cluster_shift = 6};
    CHECK_EQUAL(1u, fat_cluster_megabytes(&volume, 32));
    volume.cluster_shift = 8;
    CHECK_EQUAL(4194304u, fat_cluster_megabytes(&volume, 33554432));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(fat32_free_space_follows_a_delete),
        TEST_CASE(fat16_free_space_follows_a_delete),
        TEST_CASE(exfat_free_space_follows_a_delete),
        TEST_CASE(fat32_without_a_free_count_is_not_guessed),
        TEST_CASE(clusters_turn_into_megabytes),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
