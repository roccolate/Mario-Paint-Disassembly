#include "mpaint_music.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static const uint8_t music_tool_sample_index[15] = {
    0x0D, 0x01, 0x0F, 0x04, 0x0C,
    0x08, 0x03, 0x02, 0x07, 0x09,
    0x0E, 0x06, 0x00, 0x05, 0x0A
};

static void set_error(char *error, size_t error_size, const char *message)
{
    if (error != NULL && error_size > 0) {
        (void)snprintf(error, error_size, "%s", message);
    }
}

uint16_t mpaint_music_read_u16le(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t read_u32_words(const uint8_t *p)
{
    uint32_t lo = mpaint_music_read_u16le(p);
    uint32_t hi = mpaint_music_read_u16le(p + 2);
    return lo | (hi << 16);
}

uint32_t mpaint_music_expected_tempo_increment(uint16_t tempo_raw)
{
    return ((uint32_t)tempo_raw + UINT32_C(0x000E)) * MPAINT_MUSIC_TEMPO_MULTIPLIER;
}

bool mpaint_music_song_end_to_steps(uint16_t song_end, unsigned *steps_out)
{
    unsigned steps;

    if (song_end < 0x0018u || song_end > 0x0310u) {
        return false;
    }
    if (((unsigned)song_end - 0x0010u) % 8u != 0u) {
        return false;
    }

    steps = ((unsigned)song_end - 0x0010u) / 8u;
    if (steps < 1u || steps > MPAINT_MUSIC_MAX_STEPS) {
        return false;
    }

    if (steps_out != NULL) {
        *steps_out = steps;
    }
    return true;
}

uint8_t mpaint_music_sample_index(uint8_t instrument)
{
    if (instrument >= (uint8_t)(sizeof(music_tool_sample_index) / sizeof(music_tool_sample_index[0]))) {
        return UINT8_C(0xFF);
    }
    return music_tool_sample_index[instrument];
}

uint8_t mpaint_music_spc_voice(unsigned slot)
{
    if (slot >= MPAINT_MUSIC_SLOTS_PER_STEP) {
        return UINT8_C(0xFF);
    }
    return (uint8_t)(5u + slot);
}

MpaintMusicEvent mpaint_music_decode_event(uint16_t raw)
{
    MpaintMusicEvent event;
    uint8_t hi = (uint8_t)(raw >> 8);
    uint8_t lo = (uint8_t)raw;

    event.raw = raw;
    event.empty = (raw & UINT16_C(0x8000)) != 0u;
    event.canonical_empty = raw == MPAINT_MUSIC_EMPTY_EVENT;
    event.pitch = (uint8_t)(lo & 0x0Fu);
    event.instrument = (uint8_t)(hi & 0x0Fu);
    event.sample_index = mpaint_music_sample_index(event.instrument);
    event.flags = (uint8_t)(hi & 0xF0u);
    event.highlight = (hi & MPAINT_MUSIC_HIGHLIGHT_FLAG) != 0u;
    event.command = event.empty
        ? 0u
        : (uint8_t)((((unsigned)event.instrument + 1u) << 4) | event.pitch);

    return event;
}

MpaintMusicSettings mpaint_music_decode_settings(const uint8_t blob[MPAINT_MUSIC_BLOB_SIZE])
{
    MpaintMusicSettings settings;
    unsigned steps = 0;

    settings.song_end = mpaint_music_read_u16le(blob + MPAINT_MUSIC_OFF_SONG_END);
    settings.loop_raw = mpaint_music_read_u16le(blob + MPAINT_MUSIC_OFF_LOOP);
    settings.tempo_raw = mpaint_music_read_u16le(blob + MPAINT_MUSIC_OFF_TEMPO_RAW);
    settings.tempo_increment = read_u32_words(blob + MPAINT_MUSIC_OFF_TEMPO_INCREMENT_LO);
    settings.playback_phase = read_u32_words(blob + MPAINT_MUSIC_OFF_PLAYBACK_PHASE_LO);
    settings.meter_raw = mpaint_music_read_u16le(blob + MPAINT_MUSIC_OFF_METER);

    settings.song_end_valid = mpaint_music_song_end_to_steps(settings.song_end, &steps);
    settings.step_count = settings.song_end_valid ? steps : 0u;
    settings.loop_enabled = settings.loop_raw == 1u;
    settings.tempo_valid = settings.tempo_raw <= 0x009Fu;
    settings.tempo_increment_matches =
        settings.tempo_valid &&
        settings.tempo_increment == mpaint_music_expected_tempo_increment(settings.tempo_raw);
    settings.meter_valid = settings.meter_raw <= 1u;
    settings.beats_per_measure = settings.meter_valid ? (unsigned)settings.meter_raw + 3u : 0u;

    return settings;
}

uint16_t mpaint_music_event_raw(const uint8_t blob[MPAINT_MUSIC_BLOB_SIZE], unsigned step, unsigned slot)
{
    size_t offset;

    if (step >= MPAINT_MUSIC_MAX_STEPS || slot >= MPAINT_MUSIC_SLOTS_PER_STEP) {
        return MPAINT_MUSIC_EMPTY_EVENT;
    }

    offset = ((size_t)step * MPAINT_MUSIC_SLOTS_PER_STEP + slot) * MPAINT_MUSIC_EVENT_SIZE;
    return mpaint_music_read_u16le(blob + offset);
}

size_t mpaint_music_count_active_events(const uint8_t blob[MPAINT_MUSIC_BLOB_SIZE], unsigned active_steps_only)
{
    MpaintMusicSettings settings = mpaint_music_decode_settings(blob);
    unsigned limit = MPAINT_MUSIC_MAX_STEPS;
    size_t count = 0;
    unsigned step;
    unsigned slot;

    if (active_steps_only != 0u && settings.song_end_valid) {
        limit = settings.step_count;
    }

    for (step = 0; step < limit; step++) {
        for (slot = 0; slot < MPAINT_MUSIC_SLOTS_PER_STEP; slot++) {
            MpaintMusicEvent event = mpaint_music_decode_event(mpaint_music_event_raw(blob, step, slot));
            if (!event.empty) {
                count++;
            }
        }
    }
    return count;
}

size_t mpaint_music_count_inactive_nonffff(const uint8_t blob[MPAINT_MUSIC_BLOB_SIZE])
{
    size_t count = 0;
    unsigned step;
    unsigned slot;

    for (step = 0; step < MPAINT_MUSIC_MAX_STEPS; step++) {
        for (slot = 0; slot < MPAINT_MUSIC_SLOTS_PER_STEP; slot++) {
            uint16_t raw = mpaint_music_event_raw(blob, step, slot);
            MpaintMusicEvent event = mpaint_music_decode_event(raw);
            if (event.empty && raw != MPAINT_MUSIC_EMPTY_EVENT) {
                count++;
            }
        }
    }
    return count;
}

size_t mpaint_music_count_changed_bytes(
    const uint8_t before[MPAINT_MUSIC_BLOB_SIZE],
    const uint8_t after[MPAINT_MUSIC_BLOB_SIZE])
{
    size_t changed = 0;
    size_t i;

    for (i = 0; i < MPAINT_MUSIC_BLOB_SIZE; i++) {
        if (before[i] != after[i]) {
            changed++;
        }
    }
    return changed;
}

int mpaint_music_validate_blob(const uint8_t blob[MPAINT_MUSIC_BLOB_SIZE], char *error, size_t error_size)
{
    MpaintMusicSettings settings = mpaint_music_decode_settings(blob);
    unsigned step;
    unsigned slot;

    if (!settings.song_end_valid) {
        set_error(error, error_size, "song-end coordinate is outside the editor-generated range or alignment");
        return -1;
    }
    if (settings.loop_raw > 1u) {
        set_error(error, error_size, "loop field is not 0 or 1");
        return -1;
    }
    if (!settings.tempo_valid) {
        set_error(error, error_size, "tempo raw value is outside 0x0000..0x009F");
        return -1;
    }
    if (!settings.tempo_increment_matches) {
        set_error(error, error_size, "stored tempo increment does not match the Music Tool derivation");
        return -1;
    }
    if (!settings.meter_valid) {
        set_error(error, error_size, "meter field is not 0 (3 beats) or 1 (4 beats)");
        return -1;
    }

    for (step = 0; step < MPAINT_MUSIC_MAX_STEPS; step++) {
        for (slot = 0; slot < MPAINT_MUSIC_SLOTS_PER_STEP; slot++) {
            MpaintMusicEvent event = mpaint_music_decode_event(mpaint_music_event_raw(blob, step, slot));
            uint8_t hi;

            if (event.empty) {
                /* Playback uses BMI, so any word with bit 15 set is inactive.
                   The editor itself turns 0xFFFF into 0xDFFF while clearing
                   its transient 0x20 cursor-highlight bit every frame. */
                continue;
            }

            hi = (uint8_t)(event.raw >> 8);
            if (event.instrument > 14u) {
                set_error(error, error_size, "active event uses instrument id 15, which is not produced by the mapped editor path");
                return -1;
            }
            if (event.pitch < 1u || event.pitch > 13u || ((uint8_t)event.raw & 0xF0u) != 0u) {
                set_error(error, error_size, "active event has a pitch outside the editor-generated 1..13 range");
                return -1;
            }
            if ((hi & 0xF0u) != 0u && (hi & 0xF0u) != MPAINT_MUSIC_HIGHLIGHT_FLAG) {
                set_error(error, error_size, "active event uses high-byte flags other than the mapped 0x20 cursor-highlight bit");
                return -1;
            }
        }
    }

    if (error != NULL && error_size > 0) {
        error[0] = '\0';
    }
    return 0;
}

void mpaint_music_print_summary(FILE *out, const uint8_t blob[MPAINT_MUSIC_BLOB_SIZE])
{
    MpaintMusicSettings s = mpaint_music_decode_settings(blob);
    uint32_t expected = mpaint_music_expected_tempo_increment(s.tempo_raw);
    size_t active = mpaint_music_count_active_events(blob, 1u);
    size_t total = mpaint_music_count_active_events(blob, 0u);
    size_t inactive_nonffff = mpaint_music_count_inactive_nonffff(blob);

    (void)fprintf(out, "Music blob size:          0x%03X (%u bytes)\n",
        MPAINT_MUSIC_BLOB_SIZE, MPAINT_MUSIC_BLOB_SIZE);
    (void)fprintf(out, "Song-end coordinate:     0x%04" PRIX16 "\n", s.song_end);
    if (s.song_end_valid) {
        (void)fprintf(out, "Active steps:            %u / %u\n", s.step_count, MPAINT_MUSIC_MAX_STEPS);
    } else {
        (void)fprintf(out, "Active steps:            INVALID\n");
    }
    (void)fprintf(out, "Loop field:              0x%04" PRIX16 " (%s)\n",
        s.loop_raw, s.loop_raw == 0u ? "off" : (s.loop_raw == 1u ? "on" : "invalid"));
    (void)fprintf(out, "Tempo raw:               0x%04" PRIX16 " (%u)\n",
        s.tempo_raw, (unsigned)s.tempo_raw);
    (void)fprintf(out, "Tempo increment:         0x%08" PRIX32 "\n", s.tempo_increment);
    (void)fprintf(out, "Expected tempo increment:0x%08" PRIX32 " (%s)\n",
        expected, s.tempo_increment_matches ? "match" : "MISMATCH");
    (void)fprintf(out, "Playback phase:          0x%08" PRIX32 "\n", s.playback_phase);
    if (s.meter_valid) {
        (void)fprintf(out, "Meter selector:          0x%04" PRIX16 " (%u beats/measure)\n",
            s.meter_raw, s.beats_per_measure);
    } else {
        (void)fprintf(out, "Meter selector:          0x%04" PRIX16 " (invalid)\n", s.meter_raw);
    }
    (void)fprintf(out, "Active events in range:  %zu\n", active);
    (void)fprintf(out, "Active events all steps: %zu\n", total);
    (void)fprintf(out, "Inactive non-FFFF words: %zu\n", inactive_nonffff);
}

void mpaint_music_print_events(FILE *out, const uint8_t blob[MPAINT_MUSIC_BLOB_SIZE])
{
    MpaintMusicSettings s = mpaint_music_decode_settings(blob);
    unsigned step;
    unsigned slot;

    for (step = 0; step < MPAINT_MUSIC_MAX_STEPS; step++) {
        bool active_step = !s.song_end_valid || step < s.step_count;

        for (slot = 0; slot < MPAINT_MUSIC_SLOTS_PER_STEP; slot++) {
            MpaintMusicEvent e = mpaint_music_decode_event(mpaint_music_event_raw(blob, step, slot));
            if (e.empty) {
                continue;
            }
            (void)fprintf(out,
                "step=%02u slot=%u active=%s raw=0x%04" PRIX16
                " voice=%u instrument=%u sample=0x%02X pitch=%u flags=0x%02X highlight=%s command=0x%02X\n",
                step, slot, active_step ? "yes" : "no", e.raw, (unsigned)mpaint_music_spc_voice(slot),
                (unsigned)e.instrument, (unsigned)e.sample_index, (unsigned)e.pitch,
                (unsigned)e.flags, e.highlight ? "yes" : "no", (unsigned)e.command);
        }
    }
}

void mpaint_music_print_csv(FILE *out, const uint8_t blob[MPAINT_MUSIC_BLOB_SIZE])
{
    MpaintMusicSettings s = mpaint_music_decode_settings(blob);
    unsigned step;
    unsigned slot;

    (void)fprintf(out, "step,slot,voice,active,raw,instrument,sample,pitch,flags,highlight,command\n");
    for (step = 0; step < MPAINT_MUSIC_MAX_STEPS; step++) {
        bool active_step = !s.song_end_valid || step < s.step_count;

        for (slot = 0; slot < MPAINT_MUSIC_SLOTS_PER_STEP; slot++) {
            MpaintMusicEvent e = mpaint_music_decode_event(mpaint_music_event_raw(blob, step, slot));
            if (e.empty) {
                continue;
            }
            (void)fprintf(out, "%u,%u,%u,%u,%04" PRIX16 ",%u,%02X,%u,%02X,%u,%02X\n",
                step, slot, (unsigned)mpaint_music_spc_voice(slot), active_step ? 1u : 0u, e.raw,
                (unsigned)e.instrument, (unsigned)e.sample_index, (unsigned)e.pitch,
                (unsigned)e.flags, e.highlight ? 1u : 0u, (unsigned)e.command);
        }
    }
}

static void print_diff_event_state(FILE *out, const char *label, MpaintMusicEvent event)
{
    if (event.empty) {
        (void)fprintf(out, "  %s raw=0x%04" PRIX16 " inactive=yes canonical=%s\n",
            label, event.raw, event.canonical_empty ? "yes" : "no");
        return;
    }

    (void)fprintf(out,
        "  %s raw=0x%04" PRIX16
        " inactive=no instrument=%u sample=0x%02X pitch=%u flags=0x%02X command=0x%02X\n",
        label, event.raw, (unsigned)event.instrument, (unsigned)event.sample_index,
        (unsigned)event.pitch, (unsigned)event.flags, (unsigned)event.command);
}

size_t mpaint_music_print_diff(
    FILE *out,
    const uint8_t before[MPAINT_MUSIC_BLOB_SIZE],
    const uint8_t after[MPAINT_MUSIC_BLOB_SIZE])
{
    static const struct {
        size_t offset;
        const char *name;
    } settings[] = {
        { MPAINT_MUSIC_OFF_SONG_END, "song_end" },
        { MPAINT_MUSIC_OFF_LOOP, "loop" },
        { MPAINT_MUSIC_OFF_TEMPO_RAW, "tempo_raw" },
        { MPAINT_MUSIC_OFF_TEMPO_INCREMENT_LO, "tempo_increment_lo" },
        { MPAINT_MUSIC_OFF_TEMPO_INCREMENT_HI, "tempo_increment_hi" },
        { MPAINT_MUSIC_OFF_PLAYBACK_PHASE_LO, "playback_phase_lo" },
        { MPAINT_MUSIC_OFF_PLAYBACK_PHASE_HI, "playback_phase_hi" },
        { MPAINT_MUSIC_OFF_METER, "meter" }
    };
    size_t changed_words = 0;
    size_t changed_bytes = mpaint_music_count_changed_bytes(before, after);
    unsigned step;
    unsigned slot;
    size_t i;

    for (step = 0; step < MPAINT_MUSIC_MAX_STEPS; step++) {
        for (slot = 0; slot < MPAINT_MUSIC_SLOTS_PER_STEP; slot++) {
            uint16_t before_raw = mpaint_music_event_raw(before, step, slot);
            uint16_t after_raw = mpaint_music_event_raw(after, step, slot);
            size_t offset;

            if (before_raw == after_raw) {
                continue;
            }

            offset = ((size_t)step * MPAINT_MUSIC_SLOTS_PER_STEP + slot) * MPAINT_MUSIC_EVENT_SIZE;
            changed_words++;
            (void)fprintf(out, "EVENT offset=0x%03zX step=%02u slot=%u voice=%u\n",
                offset, step, slot, (unsigned)mpaint_music_spc_voice(slot));
            print_diff_event_state(out, "before", mpaint_music_decode_event(before_raw));
            print_diff_event_state(out, "after ", mpaint_music_decode_event(after_raw));
        }
    }

    for (i = 0; i < sizeof(settings) / sizeof(settings[0]); i++) {
        uint16_t before_raw = mpaint_music_read_u16le(before + settings[i].offset);
        uint16_t after_raw = mpaint_music_read_u16le(after + settings[i].offset);

        if (before_raw == after_raw) {
            continue;
        }

        changed_words++;
        (void)fprintf(out, "SETTING offset=0x%03zX %-20s before=0x%04" PRIX16 " after=0x%04" PRIX16 "\n",
            settings[i].offset, settings[i].name, before_raw, after_raw);
    }

    (void)fprintf(out, "Changed bytes:            %zu\n", changed_bytes);
    (void)fprintf(out, "Changed 16-bit fields:    %zu\n", changed_words);
    return changed_words;
}
