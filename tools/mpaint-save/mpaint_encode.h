#ifndef MPAINT_ENCODE_H
#define MPAINT_ENCODE_H

#include "mpaint_save.h"

#include <stddef.h>
#include <stdint.h>

#define MPAINT_LZ_MAX_DISTANCE 0x00FFu
#define MPAINT_LZ_MAX_MATCH 0x0012u
#define MPAINT_LZ_MIN_MATCH 0x0004u

typedef struct mpaint_encode_info {
    size_t lz_size;
    uint16_t huffman_size;
    uint16_t checksum_add;
    uint16_t checksum_xor;
} mpaint_encode_info;

int mpaint_lz_encode(
    const uint8_t *input,
    size_t input_size,
    uint8_t *output,
    size_t output_capacity,
    size_t *output_size,
    char *error,
    size_t error_size);

int mpaint_huffman_encode(
    const uint8_t *input,
    size_t input_size,
    uint8_t payload[MPAINT_SRAM_PAYLOAD_SIZE],
    uint16_t *huffman_size,
    char *error,
    size_t error_size);

int mpaint_encode_sram(
    const uint8_t template_sram[MPAINT_SRAM_SIZE],
    const uint8_t composition[MPAINT_COMPOSITION_SIZE],
    uint8_t output_sram[MPAINT_SRAM_SIZE],
    mpaint_encode_info *info,
    char *error,
    size_t error_size);

#endif
