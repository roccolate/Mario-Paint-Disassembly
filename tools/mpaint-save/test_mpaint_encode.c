#include "mpaint_encode.h"

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

static int roundtrip_composition(const uint8_t *composition)
{
    uint8_t *template_sram = (uint8_t *)calloc(1, MPAINT_SRAM_SIZE);
    uint8_t *encoded_sram = (uint8_t *)malloc(MPAINT_SRAM_SIZE);
    uint8_t *decoded = (uint8_t *)malloc(MPAINT_COMPOSITION_SIZE);
    mpaint_encode_info encode_info;
    char error[256];

    CHECK(template_sram != NULL);
    CHECK(encoded_sram != NULL);
    CHECK(decoded != NULL);

    CHECK(mpaint_encode_sram(
        template_sram,
        composition,
        encoded_sram,
        &encode_info,
        error,
        sizeof(error)) == 0);
    CHECK(encode_info.huffman_size >= MPAINT_HUFFMAN_MIN_SIZE);
    CHECK(encode_info.huffman_size <= MPAINT_HUFFMAN_MAX_SIZE);
    CHECK(mpaint_validate_sram(
        encoded_sram,
        MPAINT_SRAM_SIZE,
        NULL,
        error,
        sizeof(error)) == 0);
    CHECK(mpaint_decode_sram(
        encoded_sram,
        MPAINT_SRAM_SIZE,
        decoded,
        NULL,
        error,
        sizeof(error)) == 0);
    CHECK(memcmp(composition, decoded, MPAINT_COMPOSITION_SIZE) == 0);

    free(decoded);
    free(encoded_sram);
    free(template_sram);
    return 0;
}

static int test_repeating_composition(void)
{
    uint8_t *composition = (uint8_t *)malloc(MPAINT_COMPOSITION_SIZE);

    CHECK(composition != NULL);
    memset(composition, 'A', MPAINT_COMPOSITION_SIZE);
    CHECK(roundtrip_composition(composition) == 0);
    free(composition);
    return 0;
}

static int test_patterned_composition(void)
{
    uint8_t *composition = (uint8_t *)malloc(MPAINT_COMPOSITION_SIZE);
    size_t i;

    CHECK(composition != NULL);
    for (i = 0; i < MPAINT_COMPOSITION_SIZE; ++i) {
        composition[i] = (uint8_t)((i / 32u + i / 257u) & 0x0Fu);
    }
    CHECK(roundtrip_composition(composition) == 0);
    free(composition);
    return 0;
}

static int test_huffman_all_symbols(void)
{
    uint8_t input[256];
    uint8_t *payload = (uint8_t *)malloc(MPAINT_SRAM_PAYLOAD_SIZE);
    uint8_t decoded[512];
    uint16_t huffman_size = 0;
    size_t decoded_size = 0;
    char error[256];
    size_t i;

    CHECK(payload != NULL);
    for (i = 0; i < sizeof(input); ++i) {
        input[i] = (uint8_t)i;
    }

    CHECK(mpaint_huffman_encode(
        input,
        sizeof(input),
        payload,
        &huffman_size,
        error,
        sizeof(error)) == 0);
    CHECK(mpaint_huffman_decode(
        payload,
        huffman_size,
        decoded,
        sizeof(decoded),
        &decoded_size,
        error,
        sizeof(error)) == 0);
    CHECK(decoded_size >= sizeof(input));
    CHECK(memcmp(input, decoded, sizeof(input)) == 0);

    free(payload);
    return 0;
}

static int test_template_preservation(void)
{
    uint8_t *template_sram = (uint8_t *)malloc(MPAINT_SRAM_SIZE);
    uint8_t *output_sram = (uint8_t *)malloc(MPAINT_SRAM_SIZE);
    uint8_t *composition = (uint8_t *)malloc(MPAINT_COMPOSITION_SIZE);
    mpaint_encode_info info;
    char error[256];
    size_t i;

    CHECK(template_sram != NULL);
    CHECK(output_sram != NULL);
    CHECK(composition != NULL);

    for (i = 0; i < MPAINT_SRAM_SIZE; ++i) {
        template_sram[i] = (uint8_t)(i * 37u + 11u);
    }
    memset(composition, 0x5A, MPAINT_COMPOSITION_SIZE);

    CHECK(mpaint_encode_sram(
        template_sram,
        composition,
        output_sram,
        &info,
        error,
        sizeof(error)) == 0);

    CHECK(memcmp(
        template_sram,
        output_sram,
        MPAINT_SRAM_CHECKSUM_ADD_OFFSET) == 0);
    CHECK(memcmp(
        template_sram + MPAINT_SRAM_CHECKSUM_XOR_OFFSET + 2u,
        output_sram + MPAINT_SRAM_CHECKSUM_XOR_OFFSET + 2u,
        MPAINT_SRAM_COMPRESSED_SIZE_OFFSET -
            (MPAINT_SRAM_CHECKSUM_XOR_OFFSET + 2u)) == 0);

    free(composition);
    free(output_sram);
    free(template_sram);
    return 0;
}

int main(void)
{
    CHECK(test_repeating_composition() == 0);
    CHECK(test_patterned_composition() == 0);
    CHECK(test_huffman_all_symbols() == 0);
    CHECK(test_template_preservation() == 0);
    puts("PASS: Mario Paint save encoder tests");
    return 0;
}
