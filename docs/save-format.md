# Mario Paint JU save format: verified decode path

This document records the host-side model implemented by
`tools/mpaint-save`. It distinguishes behavior directly visible in the
Mario Paint disassembly from fields whose semantics are still unknown.

## Scope

Verified target:

- Mario Paint Japan/USA ROM (`MPAINT_JU`)
- 32 KiB SRAM
- original LoROM memory map

The current host codec is read-only.

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
The results are stored at offsets `0x07C2` and `0x07C4`.

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

The game may leave padding bits at the end. If the final padding reaches a
leaf, the Huffman decoder can emit extra first-stage bytes. This is harmless in
the original loader because the LZ stage stops after reconstructing exactly
`0xBA52` bytes.

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

Back-references may overlap the destination, so repeated patterns expand in the
usual LZ manner. The original encoder searches for matches up to 18 bytes; the
decoder representation itself has seven length bits.

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

## Validation strategy

The C tests do not depend only on a second implementation of the same decoder.
They include:

- fixed checksum vectors, including a carry-producing payload word;
- literal-run LZ decoding;
- overlapping LZ back-references;
- a generated 256-symbol Huffman table;
- a synthetic, checksum-valid 32 KiB SRAM image that passes the complete
  checksum -> Huffman -> LZ pipeline and reconstructs all `0xBA52` bytes;
- corruption detection after changing a checksum-covered payload byte.

The remaining gate is a real Mario Paint `.srm`. Once that passes, the next
safe milestone is an encoder capable of rebuilding a loadable save from the
uncompressed project data.
