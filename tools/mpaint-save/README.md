# Mario Paint save codec (read-only)

`mpaint-save` is a small C11 host-side decoder for the verified Mario Paint
Japan/USA 32 KiB SRAM format.

The current milestone is intentionally read-only. It validates and decodes a
save; it does not rewrite `.srm` files yet.

## Build and test

```sh
make
make test
```

The default build uses strict warnings:

```text
-std=c11 -O2 -Wall -Wextra -Werror -pedantic
```

No external libraries are required.

## Inspect a save

```sh
./mpaint-save inspect /path/to/MarioPaint.srm
```

A valid save reports:

- the stored Huffman payload size;
- the number of bytes produced by the Huffman stage;
- the number of LZ bytes consumed to reconstruct the composition;
- stored and calculated additive/XOR checksums.

The input must currently be exactly 32768 bytes. The tool never modifies the
input save.

## Decode a project

The output directory must not already exist. This prevents accidental
replacement of an earlier extraction.

```sh
./mpaint-save decode /path/to/MarioPaint.srm /tmp/mpaint-project
```

The directory contains:

```text
composition.bin       complete 0xBA52-byte uncompressed composition
animation.bin         offsets 0x0000..0x57FF
animation-path.bin    offsets 0x5800..0x5FFF
canvas.bin            offsets 0x6000..0xB7FF
music.bin             offsets 0xB800..0xBA4F
tail.bin              offsets 0xBA50..0xBA51 (meaning still unknown)
manifest.json          extraction metadata and section boundaries
```

## Decoder correspondence

The implementation follows the disassembly rather than a generic compression
library:

- `CODE_00D1F2` writes the fixed SRAM payload and checksums.
- `CODE_01F21D` decodes the Huffman stage.
- `CODE_01EF36` expands the first-stage LZ stream to `0xBA52` bytes.
- `CODE_01EDDB` is the corresponding first-stage encoder used by the game.
- `CODE_01F03A` is the corresponding Huffman encoder used by the game.

The Huffman table occupies the first `0x800` bytes of the compressed payload.
The remaining meaningful bytes are a 16-bit, MSB-first bitstream. The stored
size at SRAM offset `0x07FE` is the total meaningful Huffman payload size.

The LZ stream is token based:

- a 16-bit value with bit 15 clear is a literal-run length, followed by that
  many literal bytes;
- a 16-bit value with bit 15 set is a back-reference, with the low byte as the
  backwards distance and bits 8..14 as the copy length.

For safety, the public CLI validates checksums before attempting either decode
stage.

## Next gate

The synthetic tests prove the host implementation is internally consistent and
exercise checksum + Huffman + overlapping LZ copies end to end. The next gate
is a real `.srm` produced by Mario Paint or an emulator. Only after that passes
should an encoder or round-trip writer be added.
