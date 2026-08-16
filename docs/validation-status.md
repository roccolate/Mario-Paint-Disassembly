# Validation status

This file records the latest executed gates so older roadmap text does not need to be interpreted as current status.

## Reproducible ROM baseline

**Passed on Bellota.**

- source branch baseline verified with native Linux Asar;
- output size: 1,048,576 bytes;
- MD5: `881d3772a3eb37a8a0fb254e940c6767`;
- SHA-256: `e842cac1a4301be196f1e137fbd1a16866d5c913f24dbca313f4dd8bd7472f45`;
- `cmp`: byte-identical to the verified Mario Paint JU ROM.

## Host save codec

**Real-save format round-trip passed on Bellota.**

A 32 KiB SRAM produced by Mario Paint under MesenCE 2.2.1 was accepted by the C decoder:

```text
SRAM size              0x8000
Huffman payload size   0x0D20
Huffman decoded bytes  0x15B0
LZ bytes consumed      0x15AF
Composition size       0xBA52
additive checksum      0xE6B1 (matched)
XOR checksum           0xD96A (matched)
```

Rebuilding the same `composition.bin` produced a different but self-consistent compressed representation:

```text
rebuilt LZ size        0x15AF
rebuilt Huffman size   0x0CF0
additive checksum      0x0E23
XOR checksum           0x82B3
```

Decoding that rebuilt SRAM reconstructed the original `0xBA52`-byte composition byte-for-byte. Compressed SRAM identity is **not** a compatibility requirement because valid LZ/Huffman representations can differ.

### Remaining save-codec runtime gate

The rebuilt `.srm` still needs to be loaded by Mario Paint itself (MesenCE or hardware) to prove runtime acceptance by the original 65C816 loader. Host-format compatibility is already demonstrated; runtime acceptance remains a separate gate.

No real `.srm` or copyrighted extracted asset is committed to the repository.

## Music Tool research

**Static mapping in progress on `research/music-tool-format`.**

Current verified boundary:

```text
0x250-byte blob
  0x000-0x23F  96 steps * 3 uint16 event slots
  0x240-0x24F  song length / loop / tempo / phase / meter fields
```

A read-only C inspector and synthetic tests are being introduced before any structured music writer. Real-save `music.bin` and the three extracted Nintendo pre-composed song blobs are the next data gates.
