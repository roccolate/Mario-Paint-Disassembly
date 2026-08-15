# Linux bit-perfect baseline

This fork has a verified Linux build/extraction baseline for the original Mario Paint Japan/USA ROM (`MPAINT_JU`). The goal of this baseline is to make future reverse engineering and modifications measurable: before changing game behavior, the unmodified source must reproduce the original ROM exactly.

## Verified environment

Validation performed on Bellota (Debian/Linux) on 2026-08-15.

- Asar: 1.91
- ROM mapping: LoROM
- ROM size: 1,048,576 bytes
- SRAM size reported by the framework: 32 KB
- Supported baseline ROM: Mario Paint (Japan, USA), headerless

Verified original ROM hashes:

```text
MD5     881d3772a3eb37a8a0fb254e940c6767
SHA256  e842cac1a4301be196f1e137fbd1a16866d5c913f24dbca313f4dd8bd7472f45
```

No ROM image or extracted copyrighted assets are included in this repository.

## Verified extraction

The Linux extractor builds the same pointer/name table used by the original `ExtractAssets.bat`, then extracts the declared ranges from a user-supplied verified ROM.

Observed baseline totals:

```text
Graphics            44 files   427520 bytes
MusicToolData        3 files     1776 bytes
UnknownData          6 files    99496 bytes
Music                3 files    21408 bytes
AudienceBRR          5 files    22352 bytes
ChantingBRR          1 files    36112 bytes
FlySwattingBRR      31 files    35472 bytes
MainBRR             34 files    33088 bytes
MusicToolBRR        23 files    36704 bytes
TitleScreenBRR      32 files    32512 bytes

TOTAL: 182 files, 746440 bytes
```

As an independent check, `GFX_038000.bin` was extracted both through Asar and as a direct LoROM byte range. Both copies were 27,648 bytes and had SHA256:

```text
9326cf9971b958c6811a045f7ac1d4008d13b24229f081668d42da3d9af79ba5
```

The two files matched byte-for-byte.

## Verified rebuild

After extracting the required assets, the complete framework sequence was run natively with Asar 1.91:

1. Initialize the SNES ROM.
2. Assemble the SPC700 engine.
3. Assemble all six sample-bank files.
4. Assemble the SNES ROM body.
5. Query optional firmware.
6. Finalize the ROM.
7. Display/check the final checksum.

Generated SPC700 intermediates during the verified run:

```text
Engine.bin                  12734 bytes
AudienceSampleBank.bin      22458 bytes
ChantingSampleBank.bin      36178 bytes
FlySwattingSampleBank.bin   43716 bytes
MainSampleBank.bin          43240 bytes
MusicToolSampleBank.bin     36982 bytes
TitleScreenSampleBank.bin   32876 bytes
```

Final framework checksum report:

```text
Original Checksum: $4B9E
Checksum:          $4B9E
```

The rebuilt ROM was exactly 1,048,576 bytes and matched the original by MD5, SHA256, and `cmp`:

```text
BIT-PERFECT: rebuilt ROM is identical to original
```

## Linux commands

Extract assets from a verified ROM:

```bash
bash MPAINT/AsarScripts/ExtractAssets.sh \
  "$HOME/tmp/mario-paint-rom/Mario Paint (Japan, USA).sfc"
```

Assemble the ROM after extraction:

```bash
bash MPAINT/Assemble_MPAINT.sh
```

Run the isolated end-to-end verification:

```bash
bash scripts/verify_linux_baseline.sh \
  "$HOME/tmp/mario-paint-rom/Mario Paint (Japan, USA).sfc"
```

The verification script creates a detached temporary worktree, extracts/builds there, checks both expected hashes and a byte-for-byte comparison, then removes the worktree. Set `KEEP_WORKTREE=1` to retain it for diagnostics.

## Scope and non-goals

The Linux baseline currently certifies only `MPAINT_JU`. The PAL build path and custom `HACK_*` maps are intentionally rejected by the Linux wrappers until they are validated independently.

The Linux wrappers do not alter the Mario Paint 65C816 code, SPC700 engine, ROM map, data tables, or original Windows batch files. They provide a reproducible native path around the existing Asar framework.

## Development rule

Future feature work should preserve this baseline as a control. A change may intentionally produce a different ROM, but an unmodified checkout of the baseline must continue to rebuild the original ROM bit-perfectly from a user-supplied verified ROM.
