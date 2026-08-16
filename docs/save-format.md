# Mario Paint JU save format: host codec model

This document records the host-side save model implemented by `tools/mpaint-save`. It distinguishes behavior established from the disassembly and executed Bellota gates from fields that remain unknown.

## Scope

Verified target:

- Mario Paint Japan/USA (`MPAINT_JU`);
- headerless 1 MiB LoROM baseline;
- 32 KiB SRAM;
- C11 host decoder/encoder on Linux.

`mpaint-save` is the reader/inspector. `mpaint-save-rebuild` is a separate explicit writer that replaces only the compressed composition fields in a validated template `.srm`.

## SRAM layout

Relative to the start of the 32 KiB save:

```text
0x0000..0x07C1   other SRAM state / incompletely mapped
0x07C2..0x07C3   additive checksum
0x07C4..0x07C5   XOR checksum
0x07C6..0x07FD   other SRAM state / incompletely mapped
0x07FE..0x07FF   meaningful Huffman payload size
0x0800..0x7FFF   fixed 0x7800-byte compressed payload region
```

The first `0x800` bytes of the payload are the serialized Huffman decode tree. The packed bitstream follows. Unused bytes in the fixed payload region remain checksum-covered.

## Checksums

The game initializes:

```text
additive = 0x7003
xor      = 0x2122
```

It processes all 16-bit words of the fixed `0x7800`-byte payload from high address to low address. The additive accumulator preserves 65816 carry between `ADC` operations. The XOR accumulator XORs every payload word. The meaningful Huffman payload size is folded into both accumulators at the end.

`CODE_00D6D3` performs the corresponding validation when loading.

## Compression pipeline

Save:

```text
0xBA52-byte composition
    -> first-stage LZ stream
    -> Huffman tree + bitstream
    -> SRAM payload
```

Load:

```text
SRAM payload
    -> Huffman decode
    -> first-stage LZ decode
    -> 0xBA52-byte composition
```

### Huffman representation

The first `0x800` payload bytes contain pairs of 16-bit child pointers. A leaf has a zero left word and its right word contains the byte value. Internal-node child pointers are offsets into the same table.

Bitstream words are little-endian 16-bit values; bits are consumed from bit 15 to bit 0. Zero selects the left child and one the right child.

The original format can decode padding into extra first-stage bytes. This is harmless because the LZ stage stops after reconstructing exactly `0xBA52` composition bytes.

The host encoder emits a compatible serialized tree/bitstream. It does not attempt to reproduce Nintendo's exact Huffman tie-breaking decisions.

### First-stage LZ representation

Each command begins with a little-endian 16-bit token.

Literal:

```text
bit 15 clear
bits 0..14 = literal count
next N bytes = literal data
```

Back-reference:

```text
bit 15 set
bits 0..7  = backwards distance
bits 8..14 = copy length
```

Back-references may overlap the destination. The mapped original encoder searches up to 18 bytes and emits a reference only for matches of at least four bytes. The host encoder uses the same maximum length, minimum match and 8-bit distance limit.

## Uncompressed composition image

The decoded composition is exactly `0xBA52` bytes:

```text
0x0000..0x57FF   animation/cell graphics region
0x5800..0x5FFF   animation path/settings
0x6000..0xB7FF   canvas region
0xB800..0xBA4F   Music Tool blob (0x250 bytes)
0xBA50..0xBA51   unidentified tail
```

`composition.bin` remains the authoritative lossless representation while these subformats are mapped.

## Writer safety model

The first `0x800` SRAM bytes contain stamps and other incompletely mapped state, so the writer does not synthesize a save from nothing. It requires a `.srm` that already passes the complete host decoder and preserves template bytes outside:

```text
0x07C2..0x07C5   checksums
0x07FE..0x07FF   Huffman payload size
0x0800..0x7FFF   compressed composition payload
```

Before writing output, `mpaint-save-rebuild` decodes the generated SRAM in memory and requires the recovered `composition.bin` to match the requested input byte-for-byte.

## Executed real-save gate

A real Mario Paint save produced under MesenCE 2.2.1 on Bellota passed the host decoder:

```text
SRAM size:              0x8000 (32768 bytes)
Huffman payload size:   0x0D20 (3360 bytes)
Huffman decoded bytes:  0x15B0 (5552 bytes)
LZ bytes consumed:      0x15AF (5551 bytes)
Composition size:       0xBA52 (47698 bytes)
Additive checksum:      stored=0xE6B1 calculated=0xE6B1
XOR checksum:           stored=0xD96A calculated=0xD96A
```

Rebuilding that exact composition produced:

```text
Rebuilt LZ size:       0x15AF (5551 bytes)
Rebuilt Huffman size:  0x0CF0 (3312 bytes)
Additive checksum:     0x0E23
XOR checksum:          0x82B3
```

The rebuilt save then decoded back to the original `0xBA52`-byte composition byte-for-byte. This proves the host read/write format model against real data.

A rebuilt `.srm` is **not expected to be byte-identical** to its source. Different legal LZ choices, Huffman trees and padding can encode the same composition.

## Remaining runtime compatibility gate

Host-format compatibility is complete. One independent gate remains: place `rebuilt.srm` where MesenCE will load it, boot Mario Paint, and verify that the original 65C816 loader accepts the checksums/compression and restores the expected composition.

That runtime test is deliberately separate from the host round-trip so a shared bug in the host encoder/decoder cannot be mistaken for original-game compatibility.

## Regression coverage

The C tests cover:

- checksum vectors, including carry behavior;
- literal-run LZ decoding;
- overlapping LZ references;
- generated 256-symbol Huffman trees;
- checksum-valid synthetic SRAM through checksum -> Huffman -> LZ;
- LZ -> Huffman -> SRAM -> decode round-trips;
- full-size repeating and patterned compositions;
- preservation of template metadata outside intentional replacement fields;
- corruption detection in checksum-covered data.

The real-save measurements above are recorded in `docs/validation-status.md`. The real `.srm` itself is not committed.
