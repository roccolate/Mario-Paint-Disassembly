#ifndef MPAINT_MUSIC_H
#define MPAINT_MUSIC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

enum {
    MPAINT_MUSIC_BLOB_SIZE = 0x250,
    MPAINT_MUSIC_EVENT_BYTES = 0x240,
    MPAINT_MUSIC_MAX_STEPS = 96,
    MPAINT_MUSIC_SLOTS_PER_STEP = 3,
    MPAINT_MUSIC_EVENT_SIZE = 2,

    MPAINT_MUSIC_OFF_SONG_END = 0x240,
    MPAINT_MUSIC_OFF_LOOP = 0x242,
    MPAINT_MUSIC_OFF_TEMPO_RAW = 0x244,
    MPAINT_MUSIC_OFF_TEMPO_INCREMENT_LO = 0x246,
    MPAINT_MUSIC_OFF_TEMPO_INCREMENT_HI = 0x248,
    MPAINT_MUSIC_OFF_PLAYBACK_PHASE_LO = 0x24A,
    MPAINT_MUSIC_OFF_PLAYBACK_PHASE_HI = 0x24C,
    MPAINT_MUSIC_OFF_METER = 0x24E,

    MPAINT_COMPOSITION_SIZE = 0xBA52,
    MPAINT_COMPOSITION_MUSIC_OFFSET = 0xB800
};

#define MPAINT_MUSIC_EMPTY_EVENT UINT16_C(0xFFFF)
#define MPAINT_MUSIC_HIGHLIGHT_FLAG UINT8_C(0x20)
#define MPAINT_MUSIC_TEMPO_MULTIPLIER UINT32_C(0x00323819)

typedef struct {
    uint16_t raw;
    uint8_t pitch;
    uint8_t instrument;
    uint8_t sample_index;
    uint8_t flags;
    bool empty;
    bool canonical_empty;
    bool highlight;
    uint8_t command;
} MpaintMusicEvent;

typedef struct {
    uint16_t song_end;
    uint16_t loop_raw;
    uint16_t tempo_raw;
    uint32_t tempo_increment;
    uint32_t playback_phase;
    uint16_t meter_raw;
    unsigned step_count;
    unsigned beats_per_measure;
    bool loop_enabled;
    bool song_end_valid;
    bool tempo_valid;
    bool tempo_increment_matches;
    bool meter_valid;
} MpaintMusicSettings;

uint16_t mpaint_music_read_u16le(const uint8_t *p);
uint32_t mpaint_music_expected_tempo_increment(uint16_t tempo_raw);
bool mpaint_music_song_end_to_steps(uint16_t song_end, unsigned *steps_out);
uint8_t mpaint_music_sample_index(uint8_t instrument);
MpaintMusicEvent mpaint_music_decode_event(uint16_t raw);
MpaintMusicSettings mpaint_music_decode_settings(const uint8_t blob[MPAINT_MUSIC_BLOB_SIZE]);
uint16_t mpaint_music_event_raw(const uint8_t blob[MPAINT_MUSIC_BLOB_SIZE], unsigned step, unsigned slot);
size_t mpaint_music_count_active_events(const uint8_t blob[MPAINT_MUSIC_BLOB_SIZE], unsigned active_steps_only);
size_t mpaint_music_count_inactive_nonffff(const uint8_t blob[MPAINT_MUSIC_BLOB_SIZE]);
int mpaint_music_validate_blob(const uint8_t blob[MPAINT_MUSIC_BLOB_SIZE], char *error, size_t error_size);
void mpaint_music_print_summary(FILE *out, const uint8_t blob[MPAINT_MUSIC_BLOB_SIZE]);
void mpaint_music_print_events(FILE *out, const uint8_t blob[MPAINT_MUSIC_BLOB_SIZE]);
void mpaint_music_print_csv(FILE *out, const uint8_t blob[MPAINT_MUSIC_BLOB_SIZE]);

#endif
