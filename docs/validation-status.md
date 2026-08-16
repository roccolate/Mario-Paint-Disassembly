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

**Read-side mapped-format validation passed on Bellota against both a real SRAM and all three Nintendo pre-composed songs.**

Validated blob model:

```text
0x250-byte blob
  0x000-0x23F  96 steps * 3 uint16 event slots
  0x240        song-end coordinate
  0x242        loop flag
  0x244        raw tempo control
  0x246-0x249  derived 32-bit tempo increment
  0x24A-0x24D  32-bit playback phase
  0x24E        meter/grouping selector
```

The real SRAM decoded cleanly through the existing save codec. Its Music Tool section matched the default empty-song model:

```text
song end               0x0310 = 96 steps
loop                    off
tempo raw               0x0050
tempo increment         0x1270992E (matched derivation)
playback phase          0x00000000
meter                   1 = 4 beats/group
active events           0
mapped-format gate      PASS
```

All three extracted Nintendo-authored pre-composed blobs passed the same validator:

```text
song 1: 96 steps, loop off, tempo 0x006F, 163 active events
song 2: 96 steps, loop on,  tempo 0x002E, 129 active events, phase 0xBB8474D9
song 3: 80 steps, loop on,  tempo 0x0050, 157 active events
```

For each pre-composed song, `active events + inactive non-FFFF words = 288`, exactly the full 96-by-3 event matrix. This confirms that non-`FFFF` inactive event words are normal persisted data and must be preserved. Song 3 also has no active events beyond its mapped 80-step end.

The next Music Tool gate is controlled one-variable editing in Mario Paint: add/remove one note, change one instrument, toggle loop, alter tempo, meter, and song end, then diff only the 0x250-byte blob. A structured writer remains deferred until those write-side semantics are isolated.
