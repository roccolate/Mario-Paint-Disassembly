#include "mpaint_encode.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static int read_exact_file(const char *path, uint8_t *buffer, size_t size)
{
    FILE *fp = fopen(path, "rb");
    size_t got;
    int extra;

    if (fp == NULL) {
        fprintf(stderr, "ERROR: cannot open %s: %s\n", path, strerror(errno));
        return -1;
    }
    got = fread(buffer, 1, size, fp);
    extra = fgetc(fp);
    if (ferror(fp)) {
        fprintf(stderr, "ERROR: failed while reading %s\n", path);
        fclose(fp);
        return -1;
    }
    fclose(fp);

    if (got != size || extra != EOF) {
        fprintf(stderr, "ERROR: %s is not exactly %u bytes\n", path, (unsigned)size);
        return -1;
    }
    return 0;
}

static int path_exists(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0;
}

static int write_new_file(const char *path, const uint8_t *data, size_t size)
{
    FILE *fp;
    int write_failed;
    int close_failed;

    if (path_exists(path)) {
        fprintf(stderr, "ERROR: output already exists; refusing to overwrite: %s\n", path);
        return -1;
    }
    fp = fopen(path, "wb");
    if (fp == NULL) {
        fprintf(stderr, "ERROR: cannot create %s: %s\n", path, strerror(errno));
        return -1;
    }

    write_failed = fwrite(data, 1, size, fp) != size;
    close_failed = fclose(fp) != 0;
    if (write_failed || close_failed) {
        fprintf(stderr, "ERROR: failed while writing %s\n", path);
        (void)remove(path);
        return -1;
    }
    return 0;
}

static int rebuild_save(
    const char *template_path,
    const char *composition_path,
    const char *output_path)
{
    uint8_t *template_sram = NULL;
    uint8_t *output_sram = NULL;
    uint8_t *composition = NULL;
    uint8_t *decoded = NULL;
    mpaint_save_info template_info;
    mpaint_encode_info encode_info;
    char error[256];
    int rc = 1;

    if (path_exists(output_path)) {
        fprintf(stderr, "ERROR: output already exists; refusing to overwrite: %s\n", output_path);
        return 1;
    }

    template_sram = (uint8_t *)malloc(MPAINT_SRAM_SIZE);
    output_sram = (uint8_t *)malloc(MPAINT_SRAM_SIZE);
    composition = (uint8_t *)malloc(MPAINT_COMPOSITION_SIZE);
    decoded = (uint8_t *)malloc(MPAINT_COMPOSITION_SIZE);
    if (template_sram == NULL || output_sram == NULL ||
        composition == NULL || decoded == NULL) {
        fprintf(stderr, "ERROR: out of memory\n");
        goto done;
    }

    if (read_exact_file(template_path, template_sram, MPAINT_SRAM_SIZE) != 0 ||
        read_exact_file(composition_path, composition, MPAINT_COMPOSITION_SIZE) != 0) {
        goto done;
    }

    if (mpaint_decode_sram(
            template_sram,
            MPAINT_SRAM_SIZE,
            decoded,
            &template_info,
            error,
            sizeof(error)) != 0) {
        fprintf(stderr, "ERROR: template save is not fully decodable: %s\n", error);
        goto done;
    }

    if (mpaint_encode_sram(
            template_sram,
            composition,
            output_sram,
            &encode_info,
            error,
            sizeof(error)) != 0) {
        fprintf(stderr, "ERROR: encode failed: %s\n", error);
        goto done;
    }

    if (mpaint_decode_sram(
            output_sram,
            MPAINT_SRAM_SIZE,
            decoded,
            NULL,
            error,
            sizeof(error)) != 0) {
        fprintf(stderr, "ERROR: generated save failed self-decode: %s\n", error);
        goto done;
    }
    if (memcmp(composition, decoded, MPAINT_COMPOSITION_SIZE) != 0) {
        fprintf(stderr, "ERROR: generated save does not reconstruct composition.bin\n");
        goto done;
    }

    if (write_new_file(output_path, output_sram, MPAINT_SRAM_SIZE) != 0) {
        goto done;
    }

    printf("Template Huffman size: 0x%04X\n", template_info.compressed_size);
    printf("Rebuilt LZ size:       0x%zX (%zu bytes)\n", encode_info.lz_size, encode_info.lz_size);
    printf("Rebuilt Huffman size:  0x%04X (%u bytes)\n", encode_info.huffman_size, encode_info.huffman_size);
    printf("Additive checksum:     0x%04X\n", encode_info.checksum_add);
    printf("XOR checksum:          0x%04X\n", encode_info.checksum_xor);
    printf("SELF-VALIDATED: rebuilt save decodes to composition.bin byte-for-byte\n");
    printf("Output: %s\n", output_path);
    rc = 0;

done:
    free(decoded);
    free(composition);
    free(output_sram);
    free(template_sram);
    return rc;
}

int main(int argc, char **argv)
{
    if (argc != 4) {
        fprintf(
            stderr,
            "Usage: %s TEMPLATE.srm COMPOSITION.bin OUTPUT.srm\n",
            argv[0]);
        return 2;
    }
    return rebuild_save(argv[1], argv[2], argv[3]);
}
