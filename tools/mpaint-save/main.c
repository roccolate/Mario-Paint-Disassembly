#include "mpaint_save.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

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
        fprintf(
            stderr,
            "ERROR: %s is not exactly %u bytes\n",
            path,
            (unsigned)size);
        return -1;
    }
    return 0;
}

static int write_file(const char *path, const uint8_t *data, size_t size)
{
    FILE *fp = fopen(path, "wb");

    if (fp == NULL) {
        fprintf(stderr, "ERROR: cannot create %s: %s\n", path, strerror(errno));
        return -1;
    }
    if (fwrite(data, 1, size, fp) != size) {
        fprintf(stderr, "ERROR: failed while writing %s\n", path);
        fclose(fp);
        return -1;
    }
    if (fclose(fp) != 0) {
        fprintf(stderr, "ERROR: failed to close %s\n", path);
        return -1;
    }
    return 0;
}

static int create_output_directory(const char *path)
{
    if (mkdir(path, 0777) != 0) {
        if (errno == EEXIST) {
            fprintf(
                stderr,
                "ERROR: output directory already exists; refusing to overwrite: %s\n",
                path);
        } else {
            fprintf(stderr, "ERROR: cannot create %s: %s\n", path, strerror(errno));
        }
        return -1;
    }
    return 0;
}

static int output_path(char *buffer, size_t buffer_size, const char *dir, const char *name)
{
    int n = snprintf(buffer, buffer_size, "%s/%s", dir, name);
    if (n < 0 || (size_t)n >= buffer_size) {
        fprintf(stderr, "ERROR: output path is too long\n");
        return -1;
    }
    return 0;
}

static void print_info(const mpaint_save_info *info)
{
    printf("SRAM size:              0x%04X (%u bytes)\n", MPAINT_SRAM_SIZE, MPAINT_SRAM_SIZE);
    printf("Huffman payload size:   0x%04X (%u bytes)\n", info->compressed_size, info->compressed_size);
    printf("Huffman decoded bytes:  0x%zX (%zu bytes)\n", info->huffman_decoded_size, info->huffman_decoded_size);
    printf("LZ bytes consumed:      0x%zX (%zu bytes)\n", info->lz_bytes_consumed, info->lz_bytes_consumed);
    printf("Composition size:       0x%04X (%u bytes)\n", MPAINT_COMPOSITION_SIZE, MPAINT_COMPOSITION_SIZE);
    printf("Additive checksum:      stored=0x%04X calculated=0x%04X\n", info->stored_checksum_add, info->calculated_checksum_add);
    printf("XOR checksum:           stored=0x%04X calculated=0x%04X\n", info->stored_checksum_xor, info->calculated_checksum_xor);
}

static int write_manifest(const char *path, const mpaint_save_info *info)
{
    FILE *fp = fopen(path, "w");

    if (fp == NULL) {
        fprintf(stderr, "ERROR: cannot create %s: %s\n", path, strerror(errno));
        return -1;
    }

    fprintf(fp, "{\n");
    fprintf(fp, "  \"format\": \"mario-paint-ju-composition-v0\",\n");
    fprintf(fp, "  \"sram_size\": %u,\n", MPAINT_SRAM_SIZE);
    fprintf(fp, "  \"huffman_payload_size\": %u,\n", info->compressed_size);
    fprintf(fp, "  \"huffman_decoded_size\": %zu,\n", info->huffman_decoded_size);
    fprintf(fp, "  \"lz_bytes_consumed\": %zu,\n", info->lz_bytes_consumed);
    fprintf(fp, "  \"composition_size\": %u,\n", MPAINT_COMPOSITION_SIZE);
    fprintf(fp, "  \"checksums\": {\n");
    fprintf(fp, "    \"additive\": \"0x%04X\",\n", info->stored_checksum_add);
    fprintf(fp, "    \"xor\": \"0x%04X\"\n", info->stored_checksum_xor);
    fprintf(fp, "  },\n");
    fprintf(fp, "  \"sections\": {\n");
    fprintf(fp, "    \"animation\": {\"offset\": 0, \"size\": %u, \"file\": \"animation.bin\"},\n", MPAINT_COMPOSITION_ANIMATION_SIZE);
    fprintf(fp, "    \"animation_path\": {\"offset\": %u, \"size\": %u, \"file\": \"animation-path.bin\"},\n", MPAINT_COMPOSITION_ANIMATION_PATH_OFFSET, MPAINT_COMPOSITION_ANIMATION_PATH_SIZE);
    fprintf(fp, "    \"canvas\": {\"offset\": %u, \"size\": %u, \"file\": \"canvas.bin\"},\n", MPAINT_COMPOSITION_CANVAS_OFFSET, MPAINT_COMPOSITION_CANVAS_SIZE);
    fprintf(fp, "    \"music\": {\"offset\": %u, \"size\": %u, \"file\": \"music.bin\"},\n", MPAINT_COMPOSITION_MUSIC_OFFSET, MPAINT_COMPOSITION_MUSIC_SIZE);
    fprintf(fp, "    \"unidentified_tail\": {\"offset\": %u, \"size\": %u, \"file\": \"tail.bin\"}\n", MPAINT_COMPOSITION_TAIL_OFFSET, MPAINT_COMPOSITION_TAIL_SIZE);
    fprintf(fp, "  }\n");
    fprintf(fp, "}\n");

    if (fclose(fp) != 0) {
        fprintf(stderr, "ERROR: failed to close %s\n", path);
        return -1;
    }
    return 0;
}

static int decode_save(const char *sram_path, const char *output_dir, int write_outputs)
{
    uint8_t *sram = NULL;
    uint8_t *composition = NULL;
    mpaint_save_info info;
    char error[256];
    char path[4096];
    int rc = 1;

    sram = (uint8_t *)malloc(MPAINT_SRAM_SIZE);
    composition = (uint8_t *)malloc(MPAINT_COMPOSITION_SIZE);
    if (sram == NULL || composition == NULL) {
        fprintf(stderr, "ERROR: out of memory\n");
        goto done;
    }
    if (read_exact_file(sram_path, sram, MPAINT_SRAM_SIZE) != 0) {
        goto done;
    }
    if (mpaint_decode_sram(
            sram,
            MPAINT_SRAM_SIZE,
            composition,
            &info,
            error,
            sizeof(error)) != 0) {
        fprintf(stderr, "ERROR: %s\n", error);
        goto done;
    }

    print_info(&info);

    if (!write_outputs) {
        rc = 0;
        goto done;
    }

    if (create_output_directory(output_dir) != 0) {
        goto done;
    }

#define WRITE_SECTION(filename, offset, size) \
    do { \
        if (output_path(path, sizeof(path), output_dir, (filename)) != 0 || \
            write_file(path, composition + (offset), (size)) != 0) { \
            goto done; \
        } \
    } while (0)

    WRITE_SECTION("composition.bin", 0, MPAINT_COMPOSITION_SIZE);
    WRITE_SECTION("animation.bin", MPAINT_COMPOSITION_ANIMATION_OFFSET, MPAINT_COMPOSITION_ANIMATION_SIZE);
    WRITE_SECTION("animation-path.bin", MPAINT_COMPOSITION_ANIMATION_PATH_OFFSET, MPAINT_COMPOSITION_ANIMATION_PATH_SIZE);
    WRITE_SECTION("canvas.bin", MPAINT_COMPOSITION_CANVAS_OFFSET, MPAINT_COMPOSITION_CANVAS_SIZE);
    WRITE_SECTION("music.bin", MPAINT_COMPOSITION_MUSIC_OFFSET, MPAINT_COMPOSITION_MUSIC_SIZE);
    WRITE_SECTION("tail.bin", MPAINT_COMPOSITION_TAIL_OFFSET, MPAINT_COMPOSITION_TAIL_SIZE);

#undef WRITE_SECTION

    if (output_path(path, sizeof(path), output_dir, "manifest.json") != 0 ||
        write_manifest(path, &info) != 0) {
        goto done;
    }

    printf("Decoded project written to: %s\n", output_dir);
    rc = 0;

done:
    free(composition);
    free(sram);
    return rc;
}

static void usage(const char *argv0)
{
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  %s inspect SAVE.srm\n", argv0);
    fprintf(stderr, "  %s decode SAVE.srm OUTPUT_DIR\n", argv0);
}

int main(int argc, char **argv)
{
    if (argc == 3 && strcmp(argv[1], "inspect") == 0) {
        return decode_save(argv[2], NULL, 0);
    }
    if (argc == 4 && strcmp(argv[1], "decode") == 0) {
        return decode_save(argv[2], argv[3], 1);
    }

    usage(argv[0]);
    return 2;
}
