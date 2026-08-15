#include "mpaint_save.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expr) \
    do { \
        if (!(expr)) { \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
            return -1; \
        } \
    } while (0)

static int test_checksum_vectors(void)
{
    uint8_t sram[MPAINT_SRAM_SIZE];
    uint16_t add;
    uint16_t xor_value;

    memset(sram, 0, sizeof(sram));
    mpaint_write_le16(sram + MPAINT_SRAM_COMPRESSED_SIZE_OFFSET, 0x0802u);
    mpaint_calculate_checksums(sram, &add, &xor_value);
    CHECK(add == 0x7805u);
    CHECK(xor_value == 0x2920u);

    mpaint_write_le16(sram + MPAINT_SRAM_SIZE - 2u, 0xFFFEu);
    mpaint_calculate_checksums(sram, &add, &xor_value);
    CHECK(add == 0x7804u);
    CHECK(xor_value == 0xD6DEu);
    return 0;
}

static int test_lz_literals(void)
{
    const uint8_t input[] = {0x05, 0x00, 'h', 'e', 'l', 'l', 'o'};
    uint8_t output[5];
    size_t consumed = 0;
    char error[128];

    CHECK(mpaint_lz_decode_exact(input, sizeof(input), output, sizeof(output), &consumed, error, sizeof(error)) == 0);
    CHECK(memcmp(output, "hello", 5) == 0);
    CHECK(consumed == sizeof(input));
    return 0;
}

static int test_lz_overlap(void)
{
    const uint8_t input[] = {
        0x03, 0x00, 'A', 'B', 'C',
        0x03, 0x86
    };
    uint8_t output[9];
    size_t consumed = 0;
    char error[128];

    CHECK(mpaint_lz_decode_exact(input, sizeof(input), output, sizeof(output), &consumed, error, sizeof(error)) == 0);
    CHECK(memcmp(output, "ABCABCABC", 9) == 0);
    CHECK(consumed == sizeof(input));
    return 0;
}

static int build_identity_huffman_tree(uint8_t *tree)
{
    uint16_t current[256];
    uint16_t next[256];
    size_t count = 256;
    uint16_t next_internal = 0x0404u;
    size_t i;

    memset(tree, 0, MPAINT_HUFFMAN_TREE_SIZE);
    for (i = 0; i < 256; ++i) {
        uint16_t leaf = (uint16_t)(4u + i * 4u);
        current[i] = leaf;
        mpaint_write_le16(tree + leaf, 0);
        mpaint_write_le16(tree + leaf + 2u, (uint16_t)i);
    }

    while (count > 2) {
        size_t next_count = 0;
        for (i = 0; i < count; i += 2) {
            CHECK(next_internal <= 0x07F8u);
            mpaint_write_le16(tree + next_internal, current[i]);
            mpaint_write_le16(tree + next_internal + 2u, current[i + 1]);
            next[next_count++] = next_internal;
            next_internal = (uint16_t)(next_internal + 4u);
        }
        memcpy(current, next, next_count * sizeof(current[0]));
        count = next_count;
    }

    mpaint_write_le16(tree + 0, current[0]);
    mpaint_write_le16(tree + 2, current[1]);
    return 0;
}

static int append_u16(uint8_t *buffer, size_t capacity, size_t *size, uint16_t value)
{
    if (*size + 2 > capacity) {
        return -1;
    }
    mpaint_write_le16(buffer + *size, value);
    *size += 2;
    return 0;
}

static int build_repeating_stage1(uint8_t *stage1, size_t capacity, size_t *stage1_size)
{
    size_t size = 0;
    size_t produced = 0;

    CHECK(append_u16(stage1, capacity, &size, 1) == 0);
    CHECK(size < capacity);
    stage1[size++] = 'A';
    produced = 1;

    while (produced < MPAINT_COMPOSITION_SIZE) {
        size_t remaining = MPAINT_COMPOSITION_SIZE - produced;
        size_t length = remaining < 18u ? remaining : 18u;
        uint16_t token = (uint16_t)(0x8000u | ((uint16_t)length << 8) | 1u);
        CHECK(append_u16(stage1, capacity, &size, token) == 0);
        produced += length;
    }

    *stage1_size = size;
    return 0;
}

static int build_synthetic_sram(uint8_t *sram)
{
    uint8_t *payload = sram + MPAINT_SRAM_PAYLOAD_OFFSET;
    uint8_t stage1[0x7000];
    size_t stage1_size = 0;
    size_t encoded_bytes;
    size_t i;
    uint16_t add;
    uint16_t xor_value;

    memset(sram, 0, MPAINT_SRAM_SIZE);
    CHECK(build_identity_huffman_tree(payload) == 0);
    CHECK(build_repeating_stage1(stage1, sizeof(stage1), &stage1_size) == 0);

    encoded_bytes = (stage1_size + 1u) & ~(size_t)1u;
    CHECK(MPAINT_HUFFMAN_TREE_SIZE + encoded_bytes <= MPAINT_HUFFMAN_MAX_SIZE);

    for (i = 0; i < encoded_bytes; i += 2) {
        uint8_t first = stage1[i];
        uint8_t second = (i + 1u < stage1_size) ? stage1[i + 1u] : 0;
        payload[MPAINT_HUFFMAN_TREE_SIZE + i] = second;
        payload[MPAINT_HUFFMAN_TREE_SIZE + i + 1u] = first;
    }

    mpaint_write_le16(
        sram + MPAINT_SRAM_COMPRESSED_SIZE_OFFSET,
        (uint16_t)(MPAINT_HUFFMAN_TREE_SIZE + encoded_bytes));
    mpaint_calculate_checksums(sram, &add, &xor_value);
    mpaint_write_le16(sram + MPAINT_SRAM_CHECKSUM_ADD_OFFSET, add);
    mpaint_write_le16(sram + MPAINT_SRAM_CHECKSUM_XOR_OFFSET, xor_value);
    return 0;
}

static int test_end_to_end(void)
{
    uint8_t *sram = (uint8_t *)malloc(MPAINT_SRAM_SIZE);
    uint8_t *composition = (uint8_t *)malloc(MPAINT_COMPOSITION_SIZE);
    mpaint_save_info info;
    char error[256];
    size_t i;

    CHECK(sram != NULL);
    CHECK(composition != NULL);
    CHECK(build_synthetic_sram(sram) == 0);
    CHECK(mpaint_decode_sram(sram, MPAINT_SRAM_SIZE, composition, &info, error, sizeof(error)) == 0);
    CHECK(info.compressed_size >= MPAINT_HUFFMAN_MIN_SIZE);
    CHECK(info.lz_bytes_consumed > 0);

    for (i = 0; i < MPAINT_COMPOSITION_SIZE; ++i) {
        CHECK(composition[i] == 'A');
    }

    sram[MPAINT_SRAM_PAYLOAD_OFFSET + 0x123] ^= 0x01u;
    CHECK(mpaint_validate_sram(sram, MPAINT_SRAM_SIZE, NULL, error, sizeof(error)) != 0);

    free(composition);
    free(sram);
    return 0;
}

int main(void)
{
    CHECK(test_checksum_vectors() == 0);
    CHECK(test_lz_literals() == 0);
    CHECK(test_lz_overlap() == 0);
    CHECK(test_end_to_end() == 0);
    puts("PASS: Mario Paint save codec tests");
    return 0;
}
