# Project roadmap

The goal is to turn the bit-perfect Mario Paint disassembly into a controlled SNES creation environment while preserving a reproducible original baseline. Preservation, format research, host tooling, and new runtime features remain separate milestones.

## Phase 0 - Reproducible ROM baseline

Status: **complete and revalidated on Bellota**.

Acceptance evidence:

- verified headerless Mario Paint Japan/USA ROM;
- native Linux asset extraction;
- native Linux Asar build;
- 1,048,576-byte rebuilt ROM;
- matching MD5 and SHA-256;
- byte-identical `cmp` against the verified original.

See `docs/linux-baseline.md` and `docs/validation-status.md`.

## Phase 1 - Linux toolchain

Status: **complete and locally validated**.

Repository tooling includes:

- `MPAINT/AsarScripts/ExtractAssets.sh`
- `MPAINT/Assemble_MPAINT.sh`
- `scripts/verify_linux_baseline.sh`
- Git ignore rules for ROMs, extracted assets, save files and generated intermediates.

The original Windows batch files remain as preservation/reference material.

## Phase 2 - Semantic reverse engineering

Status: **in progress**.

Current priority order:

1. save image and SRAM layout — host codec mapped and real-save round-trip passed;
2. Music Tool data format and SPC command bridge — current active research;
3. animation/path representation;
4. canvas/tile representation;
5. mouse/UI state machine.

The project does not attempt to rename every `CODE_xxxxxx` label. Names are promoted only when runtime behavior supports them.

## Phase 3 - Host-side save/project round-trip

Status: **host-format round-trip implemented and validated with a real Mario Paint save**.

Current C tools:

```text
tools/mpaint-save/mpaint-save
tools/mpaint-save/mpaint-save-rebuild
```

The decoder produces a lossless project directory containing the authoritative full composition plus split regions. The writer rebuilds the composition payload into a new `.srm` while preserving incompletely mapped SRAM metadata from a validated template save.

Real-save gate observed on Bellota:

```text
original .srm
    -> checksum validation
    -> Huffman decode
    -> LZ decode
    -> 0xBA52-byte composition.bin
    -> LZ encode
    -> Huffman encode
    -> rebuilt .srm
    -> decode again
    -> composition.bin byte-identical to original
```

The compressed `.srm` itself is **not expected to be byte-identical** because multiple valid LZ/Huffman representations can encode the same composition.

Remaining runtime gate for this phase:

- load `rebuilt.srm` in Mario Paint/MesenCE and confirm the original 65C816 loader accepts it and the composition appears correctly.

## Phase 4 - Lossless semantic project model

Goal: replace opaque subregions progressively with reversible typed representations while keeping `composition.bin` authoritative until every conversion is lossless.

Current first target: Music Tool.

Proposed durable project shape:

```text
project/
  manifest.txt
  composition.bin
  animation-region.bin
  animation-path.bin
  canvas-region.bin
  music.bin
  tail.bin
  music/
    events.csv
    settings.txt
```

No semantic export may destroy unknown or latent original data.

## Phase 5 - Music Tool expansion

Status: **format mapping in progress on `research/music-tool-format`**.

Verified starting model:

- 0x250-byte Music Tool blob;
- first 0x240 bytes = 96 timeline steps × 3 event slots × 16-bit event words;
- three simultaneous slots are routed to SPC voices 5, 6 and 7;
- event words expose instrument ID, pitch row, inactive state and a transient UI highlight bit;
- normal editor instrument IDs map to BRR sample indices through the original SPC engine;
- remaining 0x10 bytes hold song end, loop, tempo/derived timing, playback phase and meter/grouping state.

Order of work:

1. validate the mapped model against the real Bellota save and all three Nintendo pre-composed songs;
2. controlled one-variable save diffs for note, instrument, loop, tempo, meter and song end;
3. map instrument icons/names without guessing;
4. define a human-readable lossless representation;
5. add a writer only after read-side semantics are stable;
6. host-side note/instrument/tempo editing;
7. investigate custom BRR sample banks and expanded limits;
8. only then consider extending the in-ROM Music Tool UI.

## Phase 6 - First controlled ROM modification

Goal: prove that a deliberate behavioral/visual modification can coexist with the preservation baseline.

Good candidates remain small and observable:

- one palette entry;
- one tilemap entry;
- one isolated Music Tool UI value;
- one cursor/tool behavior change.

Acceptance:

- modified ROM boots normally;
- intended change is observable;
- binary diff is bounded and understood;
- preservation baseline branch remains bit-perfect.

## Phase 7 - Graphics and animation authoring

Host-side capabilities can precede ROM UI changes:

- canvas export/import;
- SNES tile and palette conversion;
- animation frame extraction/import;
- animation path metadata inspection.

Then the SNES UI can expose richer sprite/tile/animation tools while preserving Mario Paint's mouse-oriented interaction.

## Phase 8 - Project data beyond original Mario Paint

New project/game data should be explicitly versioned rather than hidden inside unknown original bytes.

Candidate sections:

```text
maps/
actors/
objects/
events/
assets/
```

A generic intermediate model may feed more than one controlled engine/exporter without pretending arbitrary commercial SNES games share compatible formats.

## Phase 9 - Game runtime

Add a small 65C816 runtime consuming project data produced by the editor/host pipeline.

Initial vertical slice:

- tile map;
- player actor;
- object list;
- collision flags;
- simple triggers/events;
- music selection;
- editor/play transition.

Only after that works should actor behavior or visual scripting expand.

## Storage strategy

Original Mario Paint has 32 KiB SRAM and uses it densely. Early work should keep the original SRAM format and treat the host as the durable project boundary.

Practical progression:

1. original SRAM -> host project;
2. host project -> rebuilt original-compatible SRAM;
3. optional expanded SRAM for development builds;
4. host compilation into standalone ROMs;
5. flashcart/modern external storage only where it adds real value.

## Branch discipline

- `main`: preservation/upstream-compatible baseline until a deliberate integration decision;
- `research/linux-baseline`: reproducibility and early semantic anchors;
- `feat/save-codec-c`: host save codec work;
- `research/music-tool-format`: read-side Music Tool mapping;
- new feature branches only after their underlying format/routine is validated.

Do not commit original ROM images, extracted copyrighted assets, or real `.srm` files.

## Immediate gates

Current next gates are:

1. validate `tools/mpaint-music` against the real Mario Paint SRAM already produced on Bellota;
2. validate it against all three extracted pre-composed Music Tool song blobs;
3. use controlled in-game edits to isolate any fields that differ from the static model;
4. separately load the host-rebuilt `.srm` in Mario Paint to close runtime save compatibility.
