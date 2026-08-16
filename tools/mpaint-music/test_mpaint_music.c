#include "mpaint_music.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void write_u16le(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
}

static void write_u32_words(uint8_t *p, uint32_t value)
{
    write_u16le(p, (uint16_t)value);
    write_u16le(p + 2, (uint16_t)(value >> 16));
}

static void init_canonical_blob(uint8_t blob[MPAINT_MUSIC_BLOB_SIZE])
{
    uint32_t increment;

    memset(blob, 0xFF, MPAINT_MUSIC_EVENT_BYTES);
    memset(blob + MPAINT_MUSIC_EVENT_BYTES, 0, MPAINT_MUSIC_BLOB_SIZE - MPAINT_MUSIC_EVENT_BYTES);

    write_u16le(blob + MPAINT_MUSIC_OFF_SONG_END, 0x0310);
    write_u16le(blob + MPAINT_MUSIC_OFF_LOOP, 0);
    write_u16le(blob + MPAINT_MUSIC_OFF_TEMPO_RAW, 0x0050);
    increment = mpaint_music_expected_tempo_increment(0x0050);
    write_u32_words(blob + MPAINT_MUSIC_OFF_TEMPO_INCREMENT_LO, increment);
    write_u32_words(blob + MPAINT_MUSIC_OFF_PLAYBACK_PHASE_LO, 0);
    write_u16le(blob + MPAINT_MUSIC_OFF_METER, 1);
}

int main(void)
{
    uint8_t blob[MPAINT_MUSIC_BLOB_SIZE];
    MpaintMusicSettings s;
    MpaintMusicEvent e;
    char error[160];
    unsigned steps = 0;

    assert(MPAINT_MUSIC_EVENT_BYTES == MPAINT_MUSIC_MAX_STEPS *
        MPAINT_MUSIC_SLOTS_PER_STEP * MPAINT_MUSIC_EVENT_SIZE);

    assert(mpaint_music_song_end_to_steps(0x0018, &steps) && steps == 1);
    assert(mpaint_music_song_end_to_steps(0x0310, &steps) && steps == 96);
    assert(!mpaint_music_song_end_to_steps(0x0010, &steps));
    assert(!mpaint_music_song_end_to_steps(0x0311, &steps));

    assert(mpaint_music_expected_tempo_increment(0x0050) == UINT32_C(0x1270992E));
    assert(mpaint_music_sample_index(0) == 0x0D);
    assert(mpaint_music_sample_index(14) == 0x0A);
    assert(mpaint_music_sample_index(15) == 0xFF);
    assert(mpaint_music_spc_voice(0) == 5);
    assert(mpaint_music_spc_voice(1) == 6);
    assert(mpaint_music_spc_voice(2) == 7);
    assert(mpaint_music_spc_voice(3) == 0xFF);

    e = mpaint_music_decode_event(UINT16_C(0x0205));
    assert(!e.empty);
    assert(e.instrument == 2);
    assert(e.sample_index == 0x0F);
    assert(e.pitch == 5);
    assert(e.flags == 0);
    assert(!e.highlight);
    assert(e.command == 0x35);

    e = mpaint_music_decode_event(UINT16_C(0x2205));
    assert(!e.empty);
    assert(e.instrument == 2);
    assert(e.sample_index == 0x0F);
    assert(e.pitch == 5);
    assert(e.flags == 0x20);
    assert(e.highlight);
    assert(e.command == 0x35);

    e = mpaint_music_decode_event(MPAINT_MUSIC_EMPTY_EVENT);
    assert(e.empty);
    assert(e.canonical_empty);

    init_canonical_blob(blob);
    write_u16le(blob + 0, 0x0205);
    write_u16le(blob + 2, 0xFFFF);
    write_u16le(blob + 4, 0x220D);

    s = mpaint_music_decode_settings(blob);
    assert(s.song_end_valid);
    assert(s.step_count == 96);
    assert(!s.loop_enabled);
    assert(s.tempo_valid);
    assert(s.tempo_increment_matches);
    assert(s.playback_phase == 0);
    assert(s.meter_valid);
    assert(s.beats_per_measure == 4);
    assert(mpaint_music_count_active_events(blob, 1) == 2);
    assert(mpaint_music_count_inactive_nonffff(blob) == 0);
    assert(mpaint_music_validate_blob(blob, error, sizeof(error)) == 0);

    write_u16le(blob + MPAINT_MUSIC_OFF_METER, 0);
    s = mpaint_music_decode_settings(blob);
    assert(s.beats_per_measure == 3);
    assert(mpaint_music_validate_blob(blob, error, sizeof(error)) == 0);

    write_u16le(blob + MPAINT_MUSIC_OFF_LOOP, 1);
    s = mpaint_music_decode_settings(blob);
    assert(s.loop_enabled);
    assert(mpaint_music_validate_blob(blob, error, sizeof(error)) == 0);

    write_u32_words(blob + MPAINT_MUSIC_OFF_TEMPO_INCREMENT_LO, 0);
    assert(mpaint_music_validate_blob(blob, error, sizeof(error)) != 0);

    init_canonical_blob(blob);
    write_u16le(blob + 0, 0x0210);
    assert(mpaint_music_validate_blob(blob, error, sizeof(error)) != 0);

    init_canonical_blob(blob);
    write_u16le(blob + 0, 0x0F05);
    assert(mpaint_music_validate_blob(blob, error, sizeof(error)) != 0);

    init_canonical_blob(blob);
    write_u16le(blob + 0, 0x1005);
    assert(mpaint_music_validate_blob(blob, error, sizeof(error)) != 0);

    init_canonical_blob(blob);
    write_u16le(blob + 0, 0xDFFF);
    e = mpaint_music_decode_event(UINT16_C(0xDFFF));
    assert(e.empty);
    assert(!e.canonical_empty);
    assert(mpaint_music_count_active_events(blob, 0) == 0);
    assert(mpaint_music_count_inactive_nonffff(blob) == 1);
    assert(mpaint_music_validate_blob(blob, error, sizeof(error)) == 0);

    init_canonical_blob(blob);
    write_u16le(blob + 0, 0x8000);
    assert(mpaint_music_decode_event(UINT16_C(0x8000)).empty);
    assert(mpaint_music_validate_blob(blob, error, sizeof(error)) == 0);

    init_canonical_blob(blob);
    write_u16le(blob + 0, 0x0205);
    write_u16le(blob + (95u * 6u), 0x0306);
    write_u16le(blob + MPAINT_MUSIC_OFF_SONG_END, 0x0018);
    assert(mpaint_music_count_active_events(blob, 1) == 1);
    assert(mpaint_music_count_active_events(blob, 0) == 2);
    assert(mpaint_music_validate_blob(blob, error, sizeof(error)) == 0);

    puts("PASS: Mario Paint Music Tool format tests");
    return 0;
}
