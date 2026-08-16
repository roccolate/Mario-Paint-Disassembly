#include "mpaint_music.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(FILE *out)
{
    (void)fprintf(out,
        "usage:\n"
        "  mpaint-music inspect <music.bin>\n"
        "  mpaint-music validate <music.bin>\n"
        "  mpaint-music events <music.bin>\n"
        "  mpaint-music csv <music.bin>\n"
        "  mpaint-music diff <before-music.bin> <after-music.bin>\n"
        "  mpaint-music inspect-composition <composition.bin>\n"
        "  mpaint-music validate-composition <composition.bin>\n"
        "  mpaint-music events-composition <composition.bin>\n"
        "  mpaint-music csv-composition <composition.bin>\n"
        "  mpaint-music diff-composition <before-composition.bin> <after-composition.bin>\n");
}

static int read_exact(const char *path, size_t expected_size, uint8_t **data_out)
{
    FILE *fp;
    uint8_t *data;
    long size;
    size_t got;

    fp = fopen(path, "rb");
    if (fp == NULL) {
        (void)fprintf(stderr, "ERROR: could not open %s: %s\n", path, strerror(errno));
        return -1;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        (void)fprintf(stderr, "ERROR: could not seek %s\n", path);
        (void)fclose(fp);
        return -1;
    }
    size = ftell(fp);
    if (size < 0 || (size_t)size != expected_size) {
        (void)fprintf(stderr, "ERROR: %s must be exactly %zu bytes (got %ld)\n",
            path, expected_size, size);
        (void)fclose(fp);
        return -1;
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        (void)fprintf(stderr, "ERROR: could not rewind %s\n", path);
        (void)fclose(fp);
        return -1;
    }

    data = (uint8_t *)malloc(expected_size);
    if (data == NULL) {
        (void)fprintf(stderr, "ERROR: out of memory\n");
        (void)fclose(fp);
        return -1;
    }

    got = fread(data, 1, expected_size, fp);
    if (got != expected_size || fclose(fp) != 0) {
        (void)fprintf(stderr, "ERROR: could not read %s\n", path);
        free(data);
        return -1;
    }

    *data_out = data;
    return 0;
}

static int load_music_blob(const char *command, const char *path, uint8_t blob[MPAINT_MUSIC_BLOB_SIZE])
{
    bool composition = strstr(command, "-composition") != NULL;
    uint8_t *data = NULL;
    size_t expected = composition ? MPAINT_COMPOSITION_SIZE : MPAINT_MUSIC_BLOB_SIZE;

    if (read_exact(path, expected, &data) != 0) {
        return -1;
    }

    if (composition) {
        memcpy(blob, data + MPAINT_COMPOSITION_MUSIC_OFFSET, MPAINT_MUSIC_BLOB_SIZE);
    } else {
        memcpy(blob, data, MPAINT_MUSIC_BLOB_SIZE);
    }
    free(data);
    return 0;
}

static void print_diff_validation(const char *label, const uint8_t blob[MPAINT_MUSIC_BLOB_SIZE])
{
    char error[192];

    if (mpaint_music_validate_blob(blob, error, sizeof(error)) == 0) {
        (void)fprintf(stdout, "%s mapped-format validation: PASS\n", label);
    } else {
        (void)fprintf(stdout, "%s mapped-format validation: WARNING: %s\n", label, error);
    }
}

int main(int argc, char **argv)
{
    uint8_t blob[MPAINT_MUSIC_BLOB_SIZE];
    uint8_t after_blob[MPAINT_MUSIC_BLOB_SIZE];
    char error[192];
    const char *command;
    bool diff_command;

    if (argc < 2) {
        usage(stderr);
        return 2;
    }

    command = argv[1];
    diff_command = strcmp(command, "diff") == 0 || strcmp(command, "diff-composition") == 0;

    if ((diff_command && argc != 4) || (!diff_command && argc != 3)) {
        usage(stderr);
        return 2;
    }

    if (strcmp(command, "inspect") != 0 &&
        strcmp(command, "validate") != 0 &&
        strcmp(command, "events") != 0 &&
        strcmp(command, "csv") != 0 &&
        strcmp(command, "diff") != 0 &&
        strcmp(command, "inspect-composition") != 0 &&
        strcmp(command, "validate-composition") != 0 &&
        strcmp(command, "events-composition") != 0 &&
        strcmp(command, "csv-composition") != 0 &&
        strcmp(command, "diff-composition") != 0) {
        usage(stderr);
        return 2;
    }

    if (load_music_blob(command, argv[2], blob) != 0) {
        return 1;
    }

    if (diff_command) {
        if (load_music_blob(command, argv[3], after_blob) != 0) {
            return 1;
        }
        print_diff_validation("Before", blob);
        print_diff_validation("After ", after_blob);
        (void)mpaint_music_print_diff(stdout, blob, after_blob);
        return 0;
    }

    mpaint_music_print_summary(stdout, blob);
    if (mpaint_music_validate_blob(blob, error, sizeof(error)) != 0) {
        (void)fprintf(stderr, "FORMAT WARNING: %s\n", error);
        if (strncmp(command, "validate", 8) == 0) {
            return 1;
        }
    } else {
        (void)fprintf(stdout, "Mapped-format validation: PASS\n");
    }

    if (strncmp(command, "events", 6) == 0) {
        mpaint_music_print_events(stdout, blob);
    } else if (strncmp(command, "csv", 3) == 0) {
        mpaint_music_print_csv(stdout, blob);
    }

    return 0;
}
