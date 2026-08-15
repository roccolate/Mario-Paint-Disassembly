# Reverse-engineering map

This document records high-value anchors for extending Mario Paint without pretending that every unnamed routine is understood. Addresses and relationships below come from the current bit-perfect disassembly and are separated into verified observations and bounded inferences.

## Confidence levels

- **Verified:** directly visible in code/data flow or reproduced by the Linux baseline.
- **Strong inference:** multiple code paths support the interpretation, but fields are not fully decoded yet.
- **Unknown:** deliberately left unnamed until more evidence exists.

## Input, mouse, and cursor

### Verified

- `CODE_01DA0D` samples controller ports, recognizes the SNES Mouse signature, shifts the serial mouse packet into the displacement fields, and normalizes the signed X displacement.
- Mouse deltas live at `$04C6-$04C9` and the cursor position lives at `$04DC-$04E1`.
- `CODE_008B48` adds mouse X/Y displacement to the cursor coordinates while enforcing current cursor bounds.
- The drawing code already consumes the same mouse state; `CODE_00B7D6` is explicitly noted as spray-tool drawing logic and checks movement deltas.

### Why this matters

The existing mouse/cursor layer is reusable for future editor screens. A new developer UI does not need a new pointing-device driver; it can sit above the existing cursor abstraction.

## Audio bridge and Music Tool

### Verified SNES-side command queues

Four 16-byte queues exist in work RAM:

```text
queue 0  $04EC-$04FB
queue 1  $04FC-$050B
queue 2  $050C-$051B
queue 3  $051C-$052B
read indices   $052C-$052F
write indices  $0530-$0533
```

`CODE_01D308`, `CODE_01D328`, `CODE_01D348`, and `CODE_01D368` enqueue one byte into queues 0-3 respectively. Each writer advances a four-bit index and avoids advancing into the current read index.

`CODE_01DF25` is the larger SNES-to-SPC upload path. It waits for the standard `$BBAA` SPC handshake on APUIO0, streams data through the APU ports, and is used when swapping sample/music banks.

### Verified Music Tool bank

`DATA_1C8000`/`DATA_1D8000` contain the assembled `MusicToolSampleBank.bin`. Entering Music Tool paths uploads this bank through `CODE_01DF25`, then waits for an APUIO0 handshake value of `$01`.

### Verified Music Tool working blob

The Music Tool working song/settings blob begins at `$09E4` and is `0x250` bytes (592 bytes) long.

Evidence:

- all three extracted `PreComposedMusicToolSong*.bin` files are exactly 592 bytes;
- `CODE_01E96C` copies a complete pre-composed song directly into `$09E4`;
- the save path copies `$09E4..$0C33` to `$7EFC00` before compression;
- the load path copies the same `0x250` bytes back from `$7EFC00` to `$09E4`.

### Strong inference

The `0x250`-byte Music Tool blob is the best first interchange boundary for a host-side composer. We do not yet need to decode every note field to round-trip it losslessly.

## Save image before compression

### Verified size and base

`CODE_01EDDB` is marked in the original disassembly as a possible save routine. It reads from bank `$7E` beginning at `$7E4400` and processes exactly `0xBA52` bytes (47,698 bytes).

Therefore the uncompressed composition image is:

```text
$7E4400 .. $7EFE51 inclusive
size: 0xBA52
```

### Verified staging layout

Before compression, the save path performs two explicit copies:

1. `$7E3800-$7E3FFF` -> `$7E9C00-$7EA3FF` (`0x800` bytes)
2. `$09E4-$0C33` -> `$7EFC00-$7EFE4F` (`0x250` bytes)

On load, both copies are reversed after decompression.

Expressed as offsets from the uncompressed save-image base `$7E4400`:

```text
offset 0x0000  $7E4400  animation/save region begins
offset 0x5800  $7E9C00  animation path/settings staging (0x800 bytes)
offset 0x6000  $7EA400  probable saved canvas pixel region
offset 0xB800  $7EFC00  Music Tool song/settings blob (0x250 bytes)
offset 0xBA50  $7EFE50  2-byte tail not yet identified
offset 0xBA52  $7EFE52  end (exclusive)
```

The animation/path interpretation at offset `0x5800` is high confidence because the runtime animation-path area is explicitly copied there and restored from there. The `0x6000-$B7FF` canvas interpretation is a strong inference from its position inside the known canvas buffer and the surrounding verified regions; field-level canvas documentation is still pending.

## Compression pipeline

### Verified first stage

`CODE_01EDDB` walks the `0xBA52`-byte uncompressed image and emits an intermediate stream in `$7F0000`. It searches for previous matching sequences up to `0x12` bytes and emits either literals or back-reference-style records.

This is an LZ-style dictionary stage. The exact on-disk token specification still needs to be written down before we give it a public format name.

`CODE_01EF36` performs the corresponding expansion back into `$7E4400`.

### Verified second stage structure

After the first stage, `CODE_01F03A` constructs frequency/tree tables in bank `$7F` and emits a packed bitstream around `$7F2800`. `CODE_01F21D` reverses this stage during load.

The structure is consistent with a Huffman-style entropy coding stage, but the exact serialized tree/bitstream format remains a research task.

## SRAM save layout

The LoROM framework maps SRAM at `$700000`. The composition payload uses the upper `0x7800` bytes of the 32 KB SRAM:

```text
$700000-$7007C1  special-stamp data / other metadata (incompletely mapped)
$7007C2-$7007C3  additive payload checksum
$7007C4-$7007C5  XOR payload checksum
$7007C6-$7007FD  metadata not fully mapped
$7007FE-$7007FF  first-stage compressed size/length value
$700800-$707FFF  fixed 0x7800-byte compressed composition payload
```

The save routine initializes the additive checksum with `$7003` and the XOR checksum with `$2122`, folds each 16-bit payload word into those accumulators, then folds in the stored first-stage size before writing the checksum words.

### Important unknown

The first `0x800` bytes of SRAM are not fully documented. The current `SRAM_Map_MPAINT.asm` originally named only the special-stamp base. We should map this area experimentally before repurposing it or expanding the save format.

## Animation state anchors

### Verified

- runtime animation path/settings buffer: `$7E3800-$7E3FFF`;
- animation-cell graphics buffer base: `$7E4000`;
- values at `$7E3FFA`, `$7E3FFC`, and `$7E3FFE` are mirrored to working variables and restored after load;
- `$7E3FFA-$7E3FFF` are therefore persistent animation metadata inside the staged `0x800`-byte path/settings block.

### Unknown

The exact public names and units of all three final words are not committed yet. Some usage strongly suggests path length/frame-layout/speed-related state, but we should verify each field by controlled ROM edits or SRAM experiments before assigning stable names.

## Practical extension points

### 1. Lossless project export/import

The safest first host-side project format is based on the **uncompressed `0xBA52`-byte save image**, not on the compressed SRAM representation. A tool can preserve unknown bytes while exposing known sections.

A first version can split it losslessly into:

```text
project/
  composition.bin             # complete 0xBA52-byte image for exact round-trip
  animation-region.bin        # offset 0x0000..0x57FF
  animation-path.bin          # offset 0x5800..0x5FFF
  canvas-region.bin           # offset 0x6000..0xB7FF
  music-tool.bin              # offset 0xB800..0xBA4F
  tail.bin                    # offset 0xBA50..0xBA51
```

The complete `composition.bin` remains the source of truth until every field is decoded.

### 2. Music tooling

Treat the `0x250`-byte Music Tool blob as an atomic lossless format first. Then decode note placement, instrument IDs, tempo/settings, and SPC commands incrementally. This avoids blocking the project on a complete SPC700 rewrite.

### 3. New editor screens

Reuse the existing mouse packet reader, cursor coordinates, OAM cursor rendering, and tilemap/DMA infrastructure. New tools should initially be added as isolated screens rather than rewriting the drawing engine.

### 4. Game-creation layer

Do not try to reinterpret arbitrary Mario Paint bytes as a universal game engine. A future game layer should define new project sections (maps, objects, actors, events) and a small runtime while retaining Mario Paint as the editor frontend.

## Next research tasks

1. Prove the Linux wrappers still rebuild bit-perfect after the new symbolic RAM/SRAM labels.
2. Write a host-side decoder for the two compression stages and round-trip one real `.srm` without modifying it.
3. Map every byte from save offset `0xB800` through `0xBA51` and document the Music Tool blob.
4. Map animation metadata at save offsets `0x5FF8-0x5FFF` with controlled edits.
5. Map the first `0x800` bytes of SRAM and identify all stamp/save metadata.
6. Identify a minimal visual ROM edit for the first controlled modified build.
