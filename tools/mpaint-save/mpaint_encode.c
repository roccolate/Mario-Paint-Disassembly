#include "mpaint_encode.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HUFFMAN_SYMBOLS 256u
#define HUFFMAN_NODE_COUNT 511u
#define HUFFMAN_FIRST_LEAF 0x0004u
#define HUFFMAN_FIRST_INTERNAL 0x0404u

typedef struct huffman_node {
    uint32_t weight;
    uint16_t offset;
    int left;
    int right;
    unsigned symbol;
    unsigned serial;
} huffman_node;

typedef struct huffman_code {
    uint8_t bits[256];
    unsigned length;
} huffman_code;

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

static int append_bytes(
    uint8_t *output,
    size_t capacity,
    size_t *used,
    const uint8_t *input,
    size_t count)
{
    if (*used > capacity || count > capacity - *used) {
        return -1;
    }
    memcpy(output + *used, input, count);
    *used += count;
    return 0;
}

static int append_u16(
    uint8_t *output,
    size_t capacity,
    size_t *used,
    uint16_t value)
{
    uint8_t bytes[2];

    mpaint_write_le16(bytes, value);
    return append_bytes(output, capacity, used, bytes, sizeof(bytes));
}

static size_t longest_match(
    const uint8_t *input,
    size_t input_size,
    size_t position,
    size_t *distance_out)
{
    size_t max_distance;
    size_t best_length = 0;
    size_t best_distance = 0;
    size_t distance;

    max_distance = position < MPAINT_LZ_MAX_DISTANCE
        ? position
        : MPAINT_LZ_MAX_DISTANCE;

    for (distance = 1; distance <= max_distance; ++distance) {
        size_t source = position - distance;
        size_t length = 0;

        while (length < MPAINT_LZ_MAX_MATCH &&
               position + length < input_size &&
               input[source + length] == input[position + length]) {
            ++length;
        }

        if (length > best_length) {
            best_length = length;
            best_distance = distance;
            if (best_length == MPAINT_LZ_MAX_MATCH) {
                break;
            }
        }
    }

    *distance_out = best_distance;
    return best_length;
}

static int flush_literals(
    const uint8_t *input,
    size_t begin,
    size_t end,
    uint8_t *output,
    size_t output_capacity,
    size_t *used)
{
    while (begin < end) {
        size_t count = end - begin;

        if (count > 0x7FFFu) {
            count = 0x7FFFu;
        }
        if (append_u16(output, output_capacity, used, (uint16_t)count) != 0 ||
            append_bytes(output, output_capacity, used, input + begin, count) != 0) {
            return -1;
        }
        begin += count;
    }
    return 0;
}

int mpaint_lz_encode(
    const uint8_t *input,
    size_t input_size,
    uint8_t *output,
    size_t output_capacity,
    size_t *output_size,
    char *error,
    size_t error_size)
{
    size_t position = 0;
    size_t literal_start = 0;
    size_t used = 0;

    if (input == NULL || output == NULL) {
        set_error(error, error_size, "LZ input/output buffer is null");
        return -1;
    }

    while (position < input_size) {
        size_t distance = 0;
        size_t length = longest_match(
            input,
            input_size,
            position,
            &distance);

        if (length >= MPAINT_LZ_MIN_MATCH) {
            uint16_t token;

            if (flush_literals(
                    input,
                    literal_start,
                    position,
                    output,
                    output_capacity,
                    &used) != 0) {
                set_error(error, error_size, "LZ output exceeds %zu bytes", output_capacity);
                return -1;
            }

            token = (uint16_t)(
                0x8000u |
                ((uint16_t)length << 8) |
                (uint16_t)distance);
            if (append_u16(output, output_capacity, &used, token) != 0) {
                set_error(error, error_size, "LZ output exceeds %zu bytes", output_capacity);
                return -1;
            }

            position += length;
            literal_start = position;
        } else {
            ++position;
        }
    }

    if (flush_literals(
            input,
            literal_start,
            position,
            output,
            output_capacity,
            &used) != 0) {
        set_error(error, error_size, "LZ output exceeds %zu bytes", output_capacity);
        return -1;
    }

    if (output_size != NULL) {
        *output_size = used;
    }
    return 0;
}

static int node_less(const huffman_node *a, const huffman_node *b)
{
    if (a->weight != b->weight) {
        return a->weight < b->weight;
    }
    return a->serial < b->serial;
}

static int build_codes_recursive(
    const huffman_node nodes[HUFFMAN_NODE_COUNT],
    int index,
    uint8_t path[256],
    unsigned depth,
    huffman_code codes[HUFFMAN_SYMBOLS])
{
    const huffman_node *node = &nodes[index];

    if (node->left < 0 && node->right < 0) {
        if (depth == 0 || depth > 255) {
            return -1;
        }
        codes[node->symbol].length = depth;
        memcpy(codes[node->symbol].bits, path, depth);
        return 0;
    }

    if (depth >= 255 || node->left < 0 || node->right < 0) {
        return -1;
    }

    path[depth] = 0;
    if (build_codes_recursive(nodes, node->left, path, depth + 1, codes) != 0) {
        return -1;
    }
    path[depth] = 1;
    return build_codes_recursive(nodes, node->right, path, depth + 1, codes);
}

int mpaint_huffman_encode(
    const uint8_t *input,
    size_t input_size,
    uint8_t payload[MPAINT_SRAM_PAYLOAD_SIZE],
    uint16_t *huffman_size,
    char *error,
    size_t error_size)
{
    huffman_node nodes[HUFFMAN_NODE_COUNT];
    int active[HUFFMAN_NODE_COUNT];
    huffman_code codes[HUFFMAN_SYMBOLS];
    uint8_t path[256];
    size_t active_count = HUFFMAN_SYMBOLS;
    unsigned serial = 0;
    int node_count = HUFFMAN_SYMBOLS;
    uint16_t next_internal = HUFFMAN_FIRST_INTERNAL;
    size_t output_offset = MPAINT_HUFFMAN_TREE_SIZE;
    uint16_t output_word = 0;
    unsigned output_bits = 0;
    size_t i;

    if (input == NULL || payload == NULL) {
        set_error(error, error_size, "Huffman input/output buffer is null");
        return -1;
    }

    memset(payload, 0, MPAINT_SRAM_PAYLOAD_SIZE);
    memset(codes, 0, sizeof(codes));

    for (i = 0; i < HUFFMAN_SYMBOLS; ++i) {
        uint16_t leaf = (uint16_t)(HUFFMAN_FIRST_LEAF + i * 4u);

        nodes[i].weight = 0;
        nodes[i].offset = leaf;
        nodes[i].left = -1;
        nodes[i].right = -1;
        nodes[i].symbol = (unsigned)i;
        nodes[i].serial = serial++;
        active[i] = (int)i;

        mpaint_write_le16(payload + leaf, 0);
        mpaint_write_le16(payload + leaf + 2u, (uint16_t)i);
    }

    for (i = 0; i < input_size; ++i) {
        ++nodes[input[i]].weight;
    }

    while (active_count > 2) {
        size_t first = 0;
        size_t second = 1;
        size_t j;
        int left;
        int right;
        int new_index;

        if (node_less(&nodes[active[second]], &nodes[active[first]])) {
            size_t tmp = first;
            first = second;
            second = tmp;
        }

        for (j = 2; j < active_count; ++j) {
            int index = active[j];

            if (node_less(&nodes[index], &nodes[active[first]])) {
                second = first;
                first = j;
            } else if (node_less(&nodes[index], &nodes[active[second]])) {
                second = j;
            }
        }

        if (first > second) {
            size_t tmp = first;
            first = second;
            second = tmp;
        }

        left = active[first];
        right = active[second];
        if (next_internal > MPAINT_HUFFMAN_TREE_SIZE - 8u ||
            node_count >= (int)HUFFMAN_NODE_COUNT - 1) {
            set_error(error, error_size, "Huffman tree overflow");
            return -1;
        }

        new_index = node_count++;
        nodes[new_index].weight = nodes[left].weight + nodes[right].weight;
        nodes[new_index].offset = next_internal;
        nodes[new_index].left = left;
        nodes[new_index].right = right;
        nodes[new_index].symbol = 0;
        nodes[new_index].serial = serial++;

        mpaint_write_le16(payload + next_internal, nodes[left].offset);
        mpaint_write_le16(payload + next_internal + 2u, nodes[right].offset);
        next_internal = (uint16_t)(next_internal + 4u);

        active[first] = new_index;
        active[second] = active[active_count - 1];
        --active_count;
    }

    if (node_less(&nodes[active[1]], &nodes[active[0]])) {
        int tmp = active[0];
        active[0] = active[1];
        active[1] = tmp;
    }

    mpaint_write_le16(payload, nodes[active[0]].offset);
    mpaint_write_le16(payload + 2u, nodes[active[1]].offset);

    nodes[node_count].weight = 0;
    nodes[node_count].offset = 0;
    nodes[node_count].left = active[0];
    nodes[node_count].right = active[1];
    nodes[node_count].symbol = 0;
    nodes[node_count].serial = 0;

    if (build_codes_recursive(nodes, node_count, path, 0, codes) != 0) {
        set_error(error, error_size, "could not derive Huffman codes");
        return -1;
    }

    for (i = 0; i < input_size; ++i) {
        const huffman_code *code = &codes[input[i]];
        unsigned bit;

        for (bit = 0; bit < code->length; ++bit) {
            output_word = (uint16_t)((output_word << 1) | code->bits[bit]);
            ++output_bits;

            if (output_bits == 16) {
                if (output_offset + 2u > MPAINT_HUFFMAN_MAX_SIZE) {
                    set_error(
                        error,
                        error_size,
                        "Huffman payload exceeds 0x%04X bytes",
                        MPAINT_HUFFMAN_MAX_SIZE);
                    return -1;
                }
                mpaint_write_le16(payload + output_offset, output_word);
                output_offset += 2;
                output_word = 0;
                output_bits = 0;
            }
        }
    }

    if (output_bits != 0) {
        output_word = (uint16_t)(output_word << (16u - output_bits));
        if (output_offset + 2u > MPAINT_HUFFMAN_MAX_SIZE) {
            set_error(
                error,
                error_size,
                "Huffman payload exceeds 0x%04X bytes",
                MPAINT_HUFFMAN_MAX_SIZE);
            return -1;
        }
        mpaint_write_le16(payload + output_offset, output_word);
        output_offset += 2;
    }

    if (output_offset < MPAINT_HUFFMAN_MIN_SIZE) {
        mpaint_write_le16(payload + output_offset, 0);
        output_offset += 2;
    }

    if (huffman_size != NULL) {
        *huffman_size = (uint16_t)output_offset;
    }
    return 0;
}

int mpaint_encode_sram(
    const uint8_t template_sram[MPAINT_SRAM_SIZE],
    const uint8_t composition[MPAINT_COMPOSITION_SIZE],
    uint8_t output_sram[MPAINT_SRAM_SIZE],
    mpaint_encode_info *info,
    char *error,
    size_t error_size)
{
    uint8_t *stage1;
    size_t stage1_size = 0;
    uint16_t huffman_size = 0;
    uint16_t checksum_add;
    uint16_t checksum_xor;
    int rc;

    if (template_sram == NULL || composition == NULL || output_sram == NULL) {
        set_error(error, error_size, "encode input/output buffer is null");
        return -1;
    }

    stage1 = (uint8_t *)malloc(MPAINT_STAGE1_MAX_SIZE);
    if (stage1 == NULL) {
        set_error(error, error_size, "could not allocate LZ buffer");
        return -1;
    }

    rc = mpaint_lz_encode(
        composition,
        MPAINT_COMPOSITION_SIZE,
        stage1,
        MPAINT_STAGE1_MAX_SIZE,
        &stage1_size,
        error,
        error_size);
    if (rc != 0) {
        free(stage1);
        return rc;
    }

    memcpy(output_sram, template_sram, MPAINT_SRAM_SIZE);
    memset(
        output_sram + MPAINT_SRAM_PAYLOAD_OFFSET,
        0,
        MPAINT_SRAM_PAYLOAD_SIZE);

    rc = mpaint_huffman_encode(
        stage1,
        stage1_size,
        output_sram + MPAINT_SRAM_PAYLOAD_OFFSET,
        &huffman_size,
        error,
        error_size);
    free(stage1);
    if (rc != 0) {
        return rc;
    }

    mpaint_write_le16(
        output_sram + MPAINT_SRAM_COMPRESSED_SIZE_OFFSET,
        huffman_size);
    mpaint_calculate_checksums(
        output_sram,
        &checksum_add,
        &checksum_xor);
    mpaint_write_le16(
        output_sram + MPAINT_SRAM_CHECKSUM_ADD_OFFSET,
        checksum_add);
    mpaint_write_le16(
        output_sram + MPAINT_SRAM_CHECKSUM_XOR_OFFSET,
        checksum_xor);

    if (info != NULL) {
        info->lz_size = stage1_size;
        info->huffman_size = huffman_size;
        info->checksum_add = checksum_add;
        info->checksum_xor = checksum_xor;
    }
    return 0;
}
