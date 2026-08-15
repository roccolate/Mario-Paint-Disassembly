#ifndef MPAINT_SAVE_H
#define MPAINT_SAVE_H

#include <stddef.h>
#include <stdint.h>

#define MPAINT_SRAM_SIZE 0x8000u
#define MPAINT_SRAM_CHECKSUM_ADD_OFFSET 0x07C2u
#define MPAINT_SRAM_CHECKSUM_XOR_OFFSET 0x07C4u
#define MPAINT_SRAM_COMPRESSED_SIZE_OFFSET 0x07FEu
#define MPAINT_SRAM_PAYLOAD_OFFSET 0x0800u
#define MPAINT_SRAM_PAYLOAD_SIZE 0x7800u

#define MPAINT_HUFFMAN_TREE_SIZE 0x0800u
#define MPAINT_HUFFMAN_MIN_SIZE 0x0802u
#define MPAINT_HUFFMAN_MAX_SIZE 0x77FEu
#define MPAINT_STAGE1_MAX_SIZE 0xE000u

#define MPAINT_COMPOSITION_SIZE 0xBA52u
#define MPAINT_COMPOSITION_ANIMATION_OFFSET 0x0000u
#define MPAINT_COMPOSITION_ANIMATION_SIZE 0x5800u
#define MPAINT_COMPOSITION_ANIMATION_PATH_OFFSET 0x5800u
#define MPAINT_COMPOSITION_ANIMATION_PATH_SIZE 0x0800u
#define MPAINT_COMPOSITION_CANVAS_OFFSET 0x6000u
#define MPAINT_COMPOSITION_CANVAS_SIZE 0x5800u
#define MPAINT_COMPOSITION_MUSIC_OFFSET 0xB800u
#define MPAINT_COMPOSITION_MUSIC_SIZE 0x0250u
#define MPAINT_COMPOSITION_TAIL_OFFSET 0xBA50u
#define MPAINT_COMPOSITION_TAIL_SIZE 0x0002u

typedef struct mpaint_save_info {
    uint16_t stored_checksum_add;
    uint16_t stored_checksum_xor;
    uint16_t calculated_checksum_add;
    uint16_t calculated_checksum_xor;
    uint16_t compressed_size;
    size_t huffman_decoded_size;
    size_t lz_bytes_consumed;
} mpaint_save_info;

uint16_t mpaint_read_le16(const uint8_t *p);
void mpaint_write_le16(uint8_t *p, uint16_t value);

void mpaint_calculate_checksums(
    const uint8_t sram[MPAINT_SRAM_SIZE],
    uint16_t *checksum_add,
    uint16_t *checksum_xor);

int mpaint_validate_sram(
    const uint8_t *sram,
    size_t sram_size,
    mpaint_save_info *info,
    char *error,
    size_t error_size);

int mpaint_huffman_decode(
    const uint8_t *payload,
    size_t compressed_size,
    uint8_t *output,
    size_t output_capacity,
    size_t *output_size,
    char *error,
    size_t error_size);

int mpaint_lz_decode_exact(
    const uint8_t *input,
    size_t input_size,
    uint8_t *output,
    size_t output_size,
    size_t *input_consumed,
    char *error,
    size_t error_size);

int mpaint_decode_sram(
    const uint8_t *sram,
    size_t sram_size,
    uint8_t composition[MPAINT_COMPOSITION_SIZE],
    mpaint_save_info *info,
    char *error,
    size_t error_size);

#endif
