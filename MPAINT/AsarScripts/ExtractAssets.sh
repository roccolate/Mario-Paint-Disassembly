#!/usr/bin/env bash

# Native Linux asset extractor for the verified Mario Paint (Japan/USA) ROM.
# Run this script; do not source it into an interactive shell.

if [[ "${BASH_SOURCE[0]}" != "$0" ]]; then
    echo "ERROR: run this script instead of sourcing it." >&2
    return 2
fi

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
MPAINT_DIR="$(cd -- "$SCRIPT_DIR/.." && pwd)"
ROM_PATH="${1:-$SCRIPT_DIR/MPAINT.sfc}"
ROM_ID="${2:-MPAINT_JU}"

fail() {
    echo "ERROR: $*" >&2
    exit 1
}

[[ "$ROM_ID" == "MPAINT_JU" ]] || fail "only MPAINT_JU is verified by the Linux baseline"
[[ -f "$ROM_PATH" ]] || fail "ROM not found: $ROM_PATH"
command -v asar >/dev/null 2>&1 || fail "asar was not found in PATH"
command -v python3 >/dev/null 2>&1 || fail "python3 was not found in PATH"

WORK_DIR="$(mktemp -d "${TMPDIR:-/tmp}/mpaint-extract.XXXXXX")" || fail "could not create temporary directory"
cleanup() {
    rm -rf -- "$WORK_DIR"
}
trap cleanup EXIT

POINTER_ROM="$WORK_DIR/asset-pointers.sfc"

echo "===== GENERATE ASSET POINTER TABLE ====="
if ! asar \
    --fix-checksum=off \
    --no-title-check \
    --define 'ROMVer=$0001' \
    "$SCRIPT_DIR/AssetPointersAndFiles.asm" \
    "$POINTER_ROM"
then
    fail "Asar could not generate the asset pointer table"
fi

echo
echo "===== EXTRACT ASSETS ====="
python3 - "$ROM_PATH" "$POINTER_ROM" "$MPAINT_DIR" <<'PY'
import hashlib
import sys
from pathlib import Path

rom_path = Path(sys.argv[1])
pointer_path = Path(sys.argv[2])
out_root = Path(sys.argv[3])

EXPECTED_SIZE = 1_048_576
EXPECTED_MD5 = "881d3772a3eb37a8a0fb254e940c6767"
EXPECTED_SHA256 = "e842cac1a4301be196f1e137fbd1a16866d5c913f24dbca313f4dd8bd7472f45"
EXPECTED_FILES = 182
EXPECTED_BYTES = 746_440

categories = [
    (0x06, "Graphics",        "Graphics"),
    (0x0C, "MusicToolData",   "UnsortedData"),
    (0x12, "UnknownData",     "GarbageData"),
    (0x18, "Music",           "SPC700/Music"),
    (0x1E, "AudienceBRR",     "SPC700/Samples/Audience"),
    (0x24, "ChantingBRR",     "SPC700/Samples/Chanting"),
    (0x2A, "FlySwattingBRR",  "SPC700/Samples/FlySwatting"),
    (0x30, "MainBRR",         "SPC700/Samples/Main"),
    (0x36, "MusicToolBRR",    "SPC700/Samples/MusicTool"),
    (0x3C, "TitleScreenBRR",  "SPC700/Samples/TitleScreen"),
]


def u24(data: bytes, offset: int) -> int:
    if offset < 0 or offset + 3 > len(data):
        raise ValueError(f"24-bit read outside pointer table at {offset:#x}")
    return data[offset] | (data[offset + 1] << 8) | (data[offset + 2] << 16)


def lorom_to_pc(address: int) -> int:
    return ((address & 0x7F0000) >> 1) | (address & 0x7FFF)


rom = rom_path.read_bytes()
pointers = pointer_path.read_bytes()

md5 = hashlib.md5(rom).hexdigest()
sha256 = hashlib.sha256(rom).hexdigest()

if len(rom) != EXPECTED_SIZE:
    raise SystemExit(f"ROM size mismatch: {len(rom)} != {EXPECTED_SIZE}")
if md5 != EXPECTED_MD5:
    raise SystemExit(f"ROM MD5 mismatch: {md5} != {EXPECTED_MD5}")
if sha256 != EXPECTED_SHA256:
    raise SystemExit(f"ROM SHA256 mismatch: {sha256} != {EXPECTED_SHA256}")

print(f"ROM: {rom_path}")
print(f"ROM size: {len(rom)} bytes")
print(f"ROM MD5: {md5}")
print(f"ROM SHA256: {sha256}")
print()

grand_files = 0
grand_bytes = 0

for header_offset, label, relative_dir in categories:
    table_snes = u24(pointers, header_offset)
    count = u24(pointers, header_offset + 3)
    table_pc = lorom_to_pc(table_snes)
    target_dir = out_root / relative_dir
    target_dir.mkdir(parents=True, exist_ok=True)

    category_bytes = 0

    for index in range(count):
        entry = table_pc + index * 12
        start_snes = u24(pointers, entry)
        end_snes = u24(pointers, entry + 3)
        name_snes = u24(pointers, entry + 6)
        name_end_snes = u24(pointers, entry + 9)

        start_pc = lorom_to_pc(start_snes)
        end_pc = lorom_to_pc(end_snes)
        name_pc = lorom_to_pc(name_snes)
        name_end_pc = lorom_to_pc(name_end_snes)

        if not (0 <= start_pc <= end_pc <= len(rom)):
            raise SystemExit(
                f"{label}[{index}]: invalid ROM range {start_pc:#x}..{end_pc:#x}"
            )
        if not (0 <= name_pc <= name_end_pc <= len(pointers)):
            raise SystemExit(
                f"{label}[{index}]: invalid filename range {name_pc:#x}..{name_end_pc:#x}"
            )

        filename = pointers[name_pc:name_end_pc].decode("ascii")
        if not filename or Path(filename).name != filename:
            raise SystemExit(f"{label}[{index}]: unsafe filename {filename!r}")

        payload = rom[start_pc:end_pc]
        (target_dir / filename).write_bytes(payload)
        category_bytes += len(payload)

    print(f"{label:18} {count:3} files {category_bytes:8} bytes")
    grand_files += count
    grand_bytes += category_bytes

print()
print(f"TOTAL: {grand_files} files, {grand_bytes} bytes")

if grand_files != EXPECTED_FILES:
    raise SystemExit(f"asset count mismatch: {grand_files} != {EXPECTED_FILES}")
if grand_bytes != EXPECTED_BYTES:
    raise SystemExit(f"asset byte count mismatch: {grand_bytes} != {EXPECTED_BYTES}")
PY
RC=$?

if [[ "$RC" -ne 0 ]]; then
    fail "asset extraction failed"
fi

echo
echo "Extraction completed. Extracted ROM assets remain local and are ignored by Git."
