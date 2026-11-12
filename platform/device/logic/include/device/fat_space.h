#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "device/fat.h"

bool fat_free_clusters(FatVolume *volume, uint8_t *buffer, uint32_t sectors, uint32_t *free_count);
uint32_t fat_cluster_megabytes(const FatVolume *volume, uint32_t clusters);
