#include "mpaint_save.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void set_error(char *error, size_t error_size, const char *fmt, ...)
{
    va_list args;

    if (error == NULL || error_size == 0) {
        return;
    }

    va_start(args, fmt);
    (void)vsnprintf(error, error_size, fmt, args);
    va_end(args);
}

uint16_t mpaint_read_le16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

void mpaint_write_le16(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t)(value & 0xFFu);
    p[1] = (uint8_t)(value >> 8);
}

void mpaint_calculate_checksums(
    const uint8_t sram[MPAINT_SRAM_SIZE],
    uint16_t *checksum_add,
    uint16_t *checksum_xor)
{
    uint16_t add = 0x7003u;
    uint16_t xor_value = 0x2122u;
    unsigned carry = 0;
    size_t offset = MPAINT_SRAM_SIZE;

    while (offset > MPAINT_SRAM_PAYLOAD_OFFSET) {
        uint16_t word;
        uint32_t total;

        offset -= 2;
        word = mpaint_read_le16(sram + offset);
        total = (uint32_t)add + (uint32_t)word + carry;
        add = (uint16_t)total;
        carry = (unsigned)(total >> 16);
        xor_value = (uint16_t)(xor_value ^ word);
    }

    {
        uint16_t compressed_size =
            mpaint_read_le16(sram + MPAINT_SRAM_COMPRESSED_SIZE_OFFSET);
        uint32_t total = (uint32_t)add + (uint32_t)compressed_size + carry;
        add = (uint16_t)total;
        xor_value = (uint16_t)(xor_value ^ compressed_size);
    }

    if (checksum_add != NULL) {
        *checksum_add = add;
    }
    if (checksum_xor != NULL) {
        *checksum_xor = xor_value;
    }
}

int mpaint_validate_sram(
    const uint8_t *sram,
    size_t sram_size,
    mpaint_save_info *info,
    char *error,
    size_t error_size)
{
    mpaint_save_info local_info;

    if (sram == NULL) {
        set_error(error, error_size, "SRAM buffer is null");
        return -1;
    }
    if (sram_size != MPAINT_SRAM_SIZE) {
        set_error(
            error,
            error_size,
            "unexpected SRAM size: %zu bytes (expected %u)",
            sram_size,
            (unsigned)MPAINT_SRAM_SIZE);
        return -1;
    }

    memset(&local_info, 0, sizeof(local_info));
    local_info.stored_checksum_add =
        mpaint_read_le16(sram + MPAINT_SRAM_CHECKSUM_ADD_OFFSET);
    local_info.stored_checksum_xor =
        mpaint_read_le16(sram + MPAINT_SRAM_CHECKSUM_XOR_OFFSET);
    local_info.compressed_size =
        mpaint_read_le16(sram + MPAINT_SRAM_COMPRESSED_SIZE_OFFSET);

    mpaint_calculate_checksums(
        sram,
        &local_info.calculated_checksum_add,
        &local_info.calculated_checksum_xor);

    if (local_info.compressed_size < MPAINT_HUFFMAN_MIN_SIZE ||
        local_info.compressed_size > MPAINT_HUFFMAN_MAX_SIZE ||
        (local_info.compressed_size & 1u) != 0) {
        set_error(
            error,
            error_size,
            "invalid Huffman payload size: 0x%04X",
            local_info.compressed_size);
        return -1;
    }

    if (local_info.stored_checksum_add != local_info.calculated_checksum_add) {
        set_error(
            error,
            error_size,
            "additive checksum mismatch: stored 0x%04X calculated 0x%04X",
            local_info.stored_checksum_add,
            local_info.calculated_checksum_add);
        return -1;
    }

    if (local_info.stored_checksum_xor != local_info.calculated_checksum_xor) {
        set_error(
            error,
            error_size,
            "XOR checksum mismatch: stored 0x%04X calculated 0x%04X",
            local_info.stored_checksum_xor,
            local_info.calculated_checksum_xor);
        return -1;
    }

    if (info != NULL) {
        *info = local_info;
    }
    return 0;
}

static int valid_tree_node(uint16_t node)
{
    return node <= (MPAINT_HUFFMAN_TREE_SIZE - 4u) && (node & 3u) == 0;
}

int mpaint_huffman_decode(
    const uint8_t *payload,
    size_t compressed_size,
    uint8_t *output,
    size_t output_capacity,
    size_t *output_size,
    char *error,
    size_t error_size)
{
    size_t out = 0;
    size_t offset;
    uint16_t node = 0;

    if (payload == NULL || output == NULL) {
        set_error(error, error_size, "Huffman input/output buffer is null");
        return -1;
    }
    if (compressed_size < MPAINT_HUFFMAN_MIN_SIZE ||
        compressed_size > MPAINT_HUFFMAN_MAX_SIZE ||
        (compressed_size & 1u) != 0) {
        set_error(error, error_size, "invalid Huffman size: 0x%zX", compressed_size);
        return -1;
    }

    for (offset = MPAINT_HUFFMAN_TREE_SIZE; offset < compressed_size; offset += 2) {
        uint16_t bits = mpaint_read_le16(payload + offset);
        int bit_index;

        for (bit_index = 15; bit_index >= 0; --bit_index) {
            unsigned bit = (unsigned)((bits >> bit_index) & 1u);
            uint16_t child;
            uint16_t left;

            if (!valid_tree_node(node)) {
                set_error(error, error_size, "invalid Huffman node pointer: 0x%04X", node);
                return -1;
            }

            child = mpaint_read_le16(payload + node + (bit ? 2u : 0u));
            if (!valid_tree_node(child) || child == 0) {
                set_error(error, error_size, "invalid Huffman child pointer: 0x%04X", child);
                return -1;
            }

            left = mpaint_read_le16(payload + child);
            if (left == 0) {
                uint16_t symbol = mpaint_read_le16(payload + child + 2u);

                if (symbol > 0x00FFu) {
                    set_error(error, error_size, "invalid Huffman leaf symbol: 0x%04X", symbol);
                    return -1;
                }
                if (out >= output_capacity) {
                    set_error(error, error_size, "Huffman output exceeds %zu bytes", output_capacity);
                    return -1;
                }
                output[out++] = (uint8_t)symbol;
                node = 0;
            } else {
                node = child;
            }
        }
    }

    if (output_size != NULL) {
        *output_size = out;
    }
    return 0;
}

int mpaint_lz_decode_exact(
    const uint8_t *input,
    size_t input_size,
    uint8_t *output,
    size_t output_size,
    size_t *input_consumed,
    char *error,
    size_t error_size)
{
    size_t in = 0;
    size_t out = 0;

    if (input == NULL || output == NULL) {
        set_error(error, error_size, "LZ input/output buffer is null");
        return -1;
    }

    while (out < output_size) {
        uint16_t token;

        if (in + 2 > input_size) {
            set_error(error, error_size, "LZ stream ended at 0x%zX", in);
            return -1;
        }

        token = mpaint_read_le16(input + in);
        in += 2;

        if ((token & 0x8000u) != 0) {
            size_t distance = token & 0x00FFu;
            size_t length = (token >> 8) & 0x007Fu;
            size_t i;

            if (distance == 0 || distance > out) {
                set_error(
                    error,
                    error_size,
                    "invalid LZ back-reference at 0x%zX: distance=%zu output=%zu",
                    in - 2,
                    distance,
                    out);
                return -1;
            }
            if (length == 0) {
                set_error(error, error_size, "zero-length LZ back-reference at 0x%zX", in - 2);
                return -1;
            }

            for (i = 0; i < length && out < output_size; ++i) {
                output[out] = output[out - distance];
                ++out;
            }
        } else {
            size_t count = token;
            size_t needed = output_size - out;
            size_t copy_count = count < needed ? count : needed;

            if (count == 0) {
                set_error(error, error_size, "zero-length LZ literal run at 0x%zX", in - 2);
                return -1;
            }
            if (in + copy_count > input_size) {
                set_error(
                    error,
                    error_size,
                    "LZ literal run at 0x%zX exceeds decoded Huffman stream",
                    in - 2);
                return -1;
            }

            memcpy(output + out, input + in, copy_count);
            out += copy_count;
            in += copy_count;

            if (copy_count < count && out < output_size) {
                set_error(error, error_size, "internal LZ literal accounting error");
                return -1;
            }
            if (copy_count == count) {
                continue;
            }
        }
    }

    if (input_consumed != NULL) {
        *input_consumed = in;
    }
    return 0;
}

int mpaint_decode_sram(
    const uint8_t *sram,
    size_t sram_size,
    uint8_t composition[MPAINT_COMPOSITION_SIZE],
    mpaint_save_info *info,
    char *error,
    size_t error_size)
{
    mpaint_save_info local_info;
    uint8_t *stage1;
    size_t stage1_size = 0;
    size_t lz_consumed = 0;
    int rc;

    rc = mpaint_validate_sram(sram, sram_size, &local_info, error, error_size);
    if (rc != 0) {
        return rc;
    }

    stage1 = (uint8_t *)malloc(MPAINT_STAGE1_MAX_SIZE);
    if (stage1 == NULL) {
        set_error(error, error_size, "could not allocate stage-1 buffer");
        return -1;
    }

    rc = mpaint_huffman_decode(
        sram + MPAINT_SRAM_PAYLOAD_OFFSET,
        local_info.compressed_size,
        stage1,
        MPAINT_STAGE1_MAX_SIZE,
        &stage1_size,
        error,
        error_size);
    if (rc == 0) {
        rc = mpaint_lz_decode_exact(
            stage1,
            stage1_size,
            composition,
            MPAINT_COMPOSITION_SIZE,
            &lz_consumed,
            error,
            error_size);
    }

    free(stage1);

    if (rc != 0) {
        return rc;
    }

    local_info.huffman_decoded_size = stage1_size;
    local_info.lz_bytes_consumed = lz_consumed;
    if (info != NULL) {
        *info = local_info;
    }
    return 0;
}
