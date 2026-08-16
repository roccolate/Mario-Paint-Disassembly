# Mario Paint Music Tool format

This document records the mapped portion of the 0x250-byte Music Tool song/settings blob used by Mario Paint JU. The goal is a lossless host-side representation first. The read-side model has now been checked against a real Mario Paint SRAM and all three extracted Nintendo pre-composed songs; structured editing remains deferred until controlled write-side experiments isolate each mutable field.

## Scope and confidence

The runtime blob lives at WRAM `$09E4-$0C33`. The save code copies the same 0x250 bytes to composition offset `0xB800`, and the three pre-composed Music Tool songs are also exactly 0x250 bytes.

The layout below is based on direct reads/writes in bank $00, the Music Tool path through the SPC700 engine, and Bellota validation against real/Nintendo-authored data. Fields are called **verified** only when code and/or data establish their role. Exact names of the 15 instrument icons and conventional musical-note names are deliberately not guessed yet.

## Blob layout

All multi-byte values are little-endian.

```text
offset       size   meaning
0x000-0x23F  0x240  96 timeline steps * 3 event slots * uint16
0x240        2      song-end coordinate
0x242        2      loop flag (0/1)
0x244        2      raw tempo control (0x0000..0x009F)
0x246        2      derived tempo increment, low word
0x248        2      derived tempo increment, high word
0x24A        2      playback phase accumulator, low word
0x24C        2      playback phase accumulator, high word
0x24E        2      meter/grouping selector (0/1 -> 3/4 beats per group)
```

The first 0x240 bytes are therefore not three fixed tracks. Each timeline position has three **polyphony slots**. Playback reads the words at `$09E4+x`, `$09E6+x`, and `$09E8+x`, sends them through the three Music Tool command paths, then advances `x` by 6 bytes.

## Timeline length

`$0C24` is not a direct step count. The editor stores a coordinate whose mapped conversion is:

```text
steps = (song_end - 0x0010) / 8
```

Editor-generated bounds are:

```text
song_end 0x0018 -> 1 step
...
song_end 0x0310 -> 96 steps
```

Shrinking the song end does not clear later event words. Those events are latent and can become active again if the end is extended. Host tools must therefore preserve and expose all 96 steps, not only the currently active range.

The third Nintendo pre-composed song provides an independent real-data check: it stores `song_end = 0x0290`, which maps to 80 steps, and contains no active events in the remaining 16 timeline positions.

## Event word

The player treats any 16-bit event with bit 15 set as inactive (`BMI`). Active events produced by the mapped editor path use:

```text
bits  0..7   pitch byte; editor writes rows 1..13
bits  8..11  instrument id 0..14
bit      13  transient cursor-highlight flag (0x2000)
bit      15  inactive when set
```

The low pitch byte is generated from the 13 visible note rows. Its upper nibble is zero on the mapped editor path.

`0xFFFF` is the reset/deletion value, but it is **not the only normal inactive word**. `CODE_00F1FD` clears the high-byte `0x20` highlight bit on every event each frame. Applied to `0xFFFF`, that produces `0xDFFF`. Playback still skips it because bit 15 remains set. A host tool must classify inactivity by bit 15 rather than by equality with `0xFFFF`.

The three Nintendo pre-composed songs validate that rule in persisted data. For each song, the number of active events plus the number of inactive non-`FFFF` words is exactly 288, covering the full 96-by-3 event matrix. Therefore noncanonical inactive words are ordinary stored state and must not be normalized away by a lossless writer.

When a note is under the cursor, the editor ORs `0x2000` into its word. The bit is UI state, not part of the SPC command's instrument nibble.

## SNES-to-SPC command byte

For an active editor-generated event:

```text
command = ((instrument_id + 1) << 4) | pitch
```

Example:

```text
raw event 0x0205
instrument id = 2
pitch row     = 5
SPC command   = 0x35
```

The three event slots are delivered through the queue/port paths handled by `CODE_01D328`, `CODE_01D348`, and `CODE_01D368`. In Music Tool mode, the SPC engine processes those three commands through `CODE_248F`, `CODE_24A0`, and `CODE_244B`.

### SPC voice assignment

The three saved event slots are also bound to fixed SPC/DSP voices. `CODE_248F`, `CODE_24A0`, and `CODE_244B` use voice-mask bits `0x20`, `0x40`, and `0x80`; the corresponding engine cleanup routines `CODE_1236`, `CODE_1244`, and `CODE_1252` explicitly clear/set voice-state bits 5, 6, and 7. Therefore, using zero-based DSP voice numbering:

```text
saved slot 0 -> SPC voice 5
saved slot 1 -> SPC voice 6
saved slot 2 -> SPC voice 7
```

This does **not** make the slots fixed instruments. Any of the 15 Music Tool instrument IDs can be placed in any slot; the slot selects the playback voice used for that simultaneous event.

## Instrument ID to BRR sample index

`CODE_24B1` extracts the high command nibble, subtracts one, and indexes `DATA_254C`. For the 15 editor instrument IDs the mapping is:

```text
instrument id   BRR sample index
0               0x0D
1               0x01
2               0x0F
3               0x04
4               0x0C
5               0x08
6               0x03
7               0x02
8               0x07
9               0x09
10              0x0E
11              0x06
12              0x00
13              0x05
14              0x0A
```

The Music Tool sample bank contains 23 BRR files (`00` through `16`), so the 15 editor instruments do not consume the entire bank. The remaining samples are engine/support sounds and should not be assigned UI instrument names without further tracing.

`CODE_24B1` also uses the low command nibble minus one as a pitch-row index into instrument-dependent tables. This confirms the 1..13 pitch-row interpretation, but conventional note names are not yet assigned because not every Mario Paint instrument is pitched in the same musical sense.

## Loop field

`$0C26` is a verified boolean. `CODE_00F7D4` toggles it with `EOR #$0001`. During playback, reaching the song end wraps the event index to zero only when this field is non-zero.

The pre-composed data independently exercises both states: song 1 stores loop off, while songs 2 and 3 store loop on.

## Tempo fields

`$0C28` is the raw speed/tempo control. The UI clamps it to `0x0000..0x009F`; reset/default is `0x0050`.

`CODE_00F921` derives the 32-bit increment stored at `$0C2A/$0C2C`:

```text
tempo_increment = (tempo_raw + 0x000E) * 0x00323819
```

For the default value:

```text
tempo_raw       = 0x0050
increment       = 0x1270992E
```

All four validated real/Nintendo blobs contain an increment matching this derivation, including pre-composed tempos `0x006F` and `0x002E`.

Playback accumulates that increment into `$0C2E/$0C30`. Carry from the fixed-point accumulator advances timeline timing. The phase words are runtime state and are preserved losslessly even though playback resets them when starting a new run. Pre-composed song 2 notably contains a nonzero persisted phase `0xBB8474D9`, confirming that a lossless representation must preserve this field rather than assume zero.

## Meter/grouping field

`$0C32` selects one of two group sizes. `CODE_00FDFE` uses `$0C32 + 3` as its grouping divisor, and the grid paths correspond to 3- or 4-step beat groups:

```text
0 -> 3 beats per group
1 -> 4 beats per group
```

The host inspector reports this as `beats/measure`, but the raw selector remains the authoritative value until the surrounding UI semantics are fully named. The real save and all three pre-composed songs used selector `1` in the current validation set, so selector `0` remains covered by code and synthetic tests rather than this particular real-data sample.

## Real-data validation

On Bellota, `scripts/validate_music_tool_candidate.sh` passed against a real 32 KiB Mario Paint SRAM and all three extracted pre-composed song blobs.

Observed summaries:

```text
real SRAM music:
  song_end=0x0310  steps=96  loop=off  tempo=0x0050
  increment=0x1270992E  phase=0x00000000  meter=1
  active events=0

pre-composed song 1:
  song_end=0x0310  steps=96  loop=off  tempo=0x006F
  increment=0x18856435  phase=0x00000000  meter=1
  active events=163  inactive non-FFFF=125

pre-composed song 2:
  song_end=0x0310  steps=96  loop=on   tempo=0x002E
  increment=0x0BC525DC  phase=0xBB8474D9  meter=1
  active events=129  inactive non-FFFF=159

pre-composed song 3:
  song_end=0x0290  steps=80  loop=on   tempo=0x0050
  increment=0x1270992E  phase=0x00000000  meter=1
  active events=157  inactive non-FFFF=131
```

Every mapped-format validation returned `PASS`.

## Pre-composed songs

`CODE_01E93A` copies one of three complete 0x250-byte blobs from the extracted data at `DATA_02F310`, `DATA_02F560`, or `DATA_02F7B0` into `$09E4`. These are high-value fixtures because they exercise the original Nintendo-authored format without relying on generated test data. They remain local extracted assets and are not committed.

## Host inspector

`tools/mpaint-music` is read-only. It accepts either a raw 592-byte Music Tool blob or the full 47,698-byte uncompressed composition and can:

```text
inspect
validate
events
csv
inspect-composition
validate-composition
events-composition
csv-composition
```

Validation currently checks only behavior that has a mapped editor path: song-end alignment/range, loop 0/1, tempo range and derived increment, meter selector, active event pitch 1..13, instrument id 0..14, and the known transient highlight flag. Inactive words are accepted by bit-15 semantics rather than forced to `0xFFFF`.

The validation script cleans generated Music Tool build products on exit, and `.gitignore` separately excludes those build products from repository status.

## Next experiments

1. In Mario Paint, make one controlled change at a time: add one note, remove one note, change one instrument, toggle loop, alter tempo, meter, and song end.
2. Save after each change, decode the SRAM, and diff only the 0x250-byte Music Tool blob against the immediately preceding state.
3. Trace the 15 UI instrument IDs to icon/name assets without guessing names.
4. Determine which persisted event-bit changes are semantic data and which are transient editor state.
5. Only after those gates, define the structured reversible representation and add a writer for music events/settings.
