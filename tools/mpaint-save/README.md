# Mario Paint save codec

`mpaint-save` is a C11 host-side codec for the verified Mario Paint Japan/USA
32 KiB SRAM format.

It can validate and decode a save, and `mpaint-save-rebuild` can rebuild the
composition payload into a **new** save while preserving the template save's
non-composition SRAM data. Neither tool overwrites an existing output path.

## Build and test

```sh
make
make test
```

The default build uses:

```text
-std=c11 -O2 -Wall -Wextra -Werror -pedantic
```

No external libraries are required.

## Inspect

```sh
./mpaint-save inspect MarioPaint.srm
```

The save must be exactly 32768 bytes and pass the additive and XOR checksums
used by the original game.

## Decode

```sh
./mpaint-save decode MarioPaint.srm /tmp/mpaint-project
```

The output directory must not already exist. It contains:

```text
composition.bin       complete 0xBA52-byte uncompressed composition
animation.bin         offsets 0x0000..0x57FF
animation-path.bin    offsets 0x5800..0x5FFF
canvas.bin            offsets 0x6000..0xB7FF
music.bin             offsets 0xB800..0xBA4F
tail.bin              offsets 0xBA50..0xBA51
manifest.json          extraction metadata and section boundaries
```

## Rebuild a save

```sh
./mpaint-save-rebuild \
  MarioPaint-original.srm \
  /tmp/mpaint-project/composition.bin \
  /tmp/MarioPaint-rebuilt.srm
```

The rebuild tool requires a fully decodable Mario Paint save as its template.
It:

1. preserves the template SRAM outside the composition checksum/size/payload;
2. LZ-encodes the `0xBA52`-byte composition;
3. Huffman-encodes the LZ stream into the game's fixed `0x7800`-byte payload;
4. writes the Huffman payload size and both original checksum formats;
5. decodes the generated SRAM in memory and refuses to write it unless the
   resulting composition is byte-identical to the requested input.

The output path must not already exist.

The host encoder is **format compatible**, not intended to reproduce Nintendo's
compressed bytes exactly. Different valid LZ choices, Huffman tie breaking, and
unused payload bytes can produce a different `.srm` while decoding to the same
composition. The hardware/emulator load test is therefore the compatibility
gate; byte-identical compressed output is not.

## Correspondence to the disassembly

- `CODE_00D1F2`: writes payload/checksums to SRAM.
- `CODE_00D6D3`: validates those checksums on load.
- `CODE_01EDDB`: original first-stage LZ encoder.
- `CODE_01EF36`: original LZ decoder.
- `CODE_01F03A`: original Huffman encoder.
- `CODE_01F21D`: original Huffman decoder.

The host LZ encoder follows the visible format constraints used by the original:
maximum 18-byte matches, 8-bit backwards distance, and back-references only for
matches of at least four bytes.

## Safety model

- `mpaint-save inspect` and `mpaint-save decode` never modify the input save.
- `decode` refuses an existing output directory.
- `mpaint-save-rebuild` refuses a template that cannot be fully decoded.
- `mpaint-save-rebuild` refuses an existing output file.
- generated saves are self-decoded and composition-compared before being
  written.
- `.srm` files are ignored by Git in this repository.

## Remaining compatibility gate

Synthetic tests now cover both directions: checksum validation, Huffman/LZ
decode, LZ/Huffman encode, overlapping back-references, metadata preservation,
and full `composition -> SRAM -> composition` round-trips. The remaining gate
is a real `.srm` produced by Mario Paint, followed by loading the rebuilt save
in an emulator or real SNES.
