# Mario Paint JU save format: host codec model

This document records the host-side model implemented by `tools/mpaint-save`.
It distinguishes behavior directly visible in the Mario Paint disassembly from
fields whose semantics are still unknown.

## Scope

Verified target:

- Mario Paint Japan/USA ROM (`MPAINT_JU`)
- 32 KiB SRAM
- original LoROM memory map

The read path is implemented by `mpaint-save`. A separate explicit writer,
`mpaint-save-rebuild`, rebuilds only the composition payload into a new `.srm`
using a fully decodable Mario Paint save as the metadata template.

## SRAM layout used by the composition save

The save/load code copies a fixed `0x7800`-byte compressed payload between
WRAM `$7F2000` and SRAM `$700800`.

Relative to the start of the 32 KiB `.srm` file:

```text
0x0000..0x07C1   other SRAM state / not fully mapped
0x07C2..0x07C3   additive checksum
0x07C4..0x07C5   XOR checksum
0x07C6..0x07FD   other SRAM state / not fully mapped
0x07FE..0x07FF   meaningful Huffman payload size
0x0800..0x7FFF   fixed 0x7800-byte payload storage
```

The first `0x800` bytes of the payload are the Huffman decode table. The
meaningful bitstream follows it and ends at the stored payload size. Bytes
between that size and the end of SRAM remain checksum-covered because the game
writes and checks the entire fixed payload region.

## Checksums

The save routine initializes:

```text
additive = 0x7003
xor      = 0x2122
```

It walks all 16-bit words of the fixed `0x7800`-byte payload, from high address
to low address. The additive calculation preserves the 65816 carry between
`ADC` operations, making it an end-around-carry style 16-bit sum. The XOR
accumulator XORs each payload word.

Finally, the 16-bit meaningful Huffman size is folded into both accumulators.
The results are stored at offsets `0x07C2` and `0x07C4`. `CODE_00D6D3` performs
the same calculation when validating a save before loading it.

## Compression pipeline

Save:

```text
0xBA52-byte composition
    |
    | CODE_01EDDB
    v
first-stage LZ stream
    |
    | CODE_01F03A
    v
0x800-byte Huffman tree + packed bitstream
    |
    v
SRAM payload at 0x0800
```

Load performs the inverse:

```text
SRAM payload
    |
    | CODE_01F21D
    v
first-stage LZ stream
    |
    | CODE_01EF36
    v
0xBA52-byte composition
```

### Huffman representation

The first `0x800` payload bytes contain pairs of 16-bit child pointers. A leaf
is identified by a zero left word; its right word contains the byte value.
Internal-node child pointers are offsets into this same `0x800`-byte table.

The bitstream is processed as 16-bit little-endian words. Within each word,
bits are consumed from bit 15 down to bit 0. A zero selects the left child and
a one selects the right child.

The game may leave padding bits at the end. If final padding reaches a leaf,
the Huffman decoder can emit extra first-stage bytes. This is harmless because
the LZ stage stops after reconstructing exactly `0xBA52` bytes.

The host encoder builds the same serialized tree shape: all 256 byte symbols
have leaves inside the fixed `0x800`-byte table, internal nodes contain offsets
to their children, and the root pair is stored at offsets `0x0000/0x0002`.
Its tie breaking does not need to reproduce Nintendo's tree byte-for-byte; the
serialized tree and bitstream only need to be mutually compatible with the
original decoder.

### First-stage LZ representation

Each command starts with a little-endian 16-bit token.

If bit 15 is clear:

```text
bits 0..14 = literal byte count
next N bytes = literal data
```

If bit 15 is set:

```text
bits 0..7  = backwards distance
bits 8..14 = copy length
bit 15     = 1
```

Back-references may overlap the destination. The original encoder searches for
matches up to 18 bytes and only emits a back-reference when at least four bytes
match. The host encoder uses the same 18-byte maximum, four-byte minimum, and
8-bit backwards-distance limit.

## Uncompressed composition image

The reconstructed image is exactly `0xBA52` bytes:

```text
0x0000..0x57FF   animation/cell graphics region
0x5800..0x5FFF   animation path and settings
0x6000..0xB7FF   canvas region
0xB800..0xBA4F   Music Tool data (0x250 bytes)
0xBA50..0xBA51   unidentified tail
```

The boundaries at `0x5800`, `0x6000`, and `0xB800` are independently visible
in save/load copies and runtime buffers. The final two bytes remain deliberately
unnamed until their behavior is established.

## Writer safety and compatibility model

`mpaint-save-rebuild` does not construct the first `0x800` bytes of SRAM from
scratch because that area still contains incompletely mapped stamps and other
state. It instead requires an existing Mario Paint save that passes the full
host decoder and preserves that template outside these replacement fields:

```text
0x07C2..0x07C5   checksums
0x07FE..0x07FF   Huffman payload size
0x0800..0x7FFF   compressed composition payload
```

The new payload region is zeroed before encoding, then the LZ stream, Huffman
tree/bitstream, size and checksums are regenerated. Before the CLI writes an
output file, it decodes the generated SRAM in memory and requires the resulting
`0xBA52` bytes to equal the requested `composition.bin` byte-for-byte.

A rebuilt `.srm` is **not expected to be byte-identical** to the original save.
Valid LZ choices, Huffman tie breaking and unused payload bytes can differ while
representing the same composition. Compatibility is defined by successful
original-format decode and, ultimately, successful loading in Mario Paint.

## Validation strategy

The C tests cover:

- fixed checksum vectors, including a carry-producing payload word;
- literal-run LZ decoding;
- overlapping LZ back-references;
- a generated 256-symbol Huffman decode table;
- a synthetic checksum-valid SRAM image through checksum -> Huffman -> LZ;
- host LZ -> Huffman -> SRAM -> original-format host decode round-trips;
- repeating and patterned full-size `0xBA52` compositions;
- preservation of template metadata outside the fields intentionally rebuilt;
- corruption detection in checksum-covered payload data.

The remaining compatibility gate is a real `.srm` produced by Mario Paint or
an emulator. The validation script will decode it, rebuild it from its own
`composition.bin`, decode the rebuilt save again, and require both uncompressed
compositions to be identical. After that, the rebuilt `.srm` should be loaded
inside Mario Paint for the final runtime compatibility check.
