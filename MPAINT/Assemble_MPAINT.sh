#!/usr/bin/env bash

# Native Linux build wrapper for the verified Mario Paint (Japan/USA) baseline.
# Run this script; do not source it into an interactive shell.

if [[ "${BASH_SOURCE[0]}" != "$0" ]]; then
    echo "ERROR: run this script instead of sourcing it." >&2
    return 2
fi

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
ROM_ID="${1:-MPAINT_JU}"
OUTPUT_PATH="${2:-$SCRIPT_DIR/Mario Paint (JU).sfc}"

fail() {
    echo "ERROR: $*" >&2
    exit 1
}

[[ "$ROM_ID" == "MPAINT_JU" ]] || fail "only MPAINT_JU is verified by the Linux baseline"
command -v asar >/dev/null 2>&1 || fail "asar was not found in PATH"

GENERATED_SPC=(
    "$SCRIPT_DIR/SPC700/Engine.bin"
    "$SCRIPT_DIR/SPC700/TitleScreenSampleBank.bin"
    "$SCRIPT_DIR/SPC700/AudienceSampleBank.bin"
    "$SCRIPT_DIR/SPC700/ChantingSampleBank.bin"
    "$SCRIPT_DIR/SPC700/MainSampleBank.bin"
    "$SCRIPT_DIR/SPC700/MusicToolSampleBank.bin"
    "$SCRIPT_DIR/SPC700/FlySwattingSampleBank.bin"
)
TEMP_FILE="$SCRIPT_DIR/Temp.txt"

cleanup() {
    rm -f -- "$TEMP_FILE" "${GENERATED_SPC[@]}"
}
trap cleanup EXIT

run_asar() {
    local label="$1"
    shift

    echo
    echo "===== $label ====="
    "$@"
    local rc=$?
    if [[ "$rc" -ne 0 ]]; then
        echo "ERROR: $label failed with exit code $rc" >&2
        return "$rc"
    fi
    return 0
}

cd "$SCRIPT_DIR" || fail "could not enter $SCRIPT_DIR"
rm -f -- "$OUTPUT_PATH" "$TEMP_FILE" "${GENERATED_SPC[@]}"

run_asar "INITIALIZE ROM" \
    asar \
    --fix-checksum=on \
    --define GameID=MPAINT \
    --define ROMID="$ROM_ID" \
    --define FileType=0 \
    ../Global/AssembleFile.asm \
    "$OUTPUT_PATH" || exit 1

run_asar "ASSEMBLE SPC700 ENGINE" \
    asar \
    --no-title-check \
    --define GameID=MPAINT \
    --define ROMID="$ROM_ID" \
    --define FileType=4 \
    --define PathToFile=SPC700/Engine.asm \
    ../Global/AssembleFile.asm \
    SPC700/Engine.bin || exit 1

for bank in \
    TitleScreenSampleBank \
    AudienceSampleBank \
    ChantingSampleBank \
    MainSampleBank \
    MusicToolSampleBank \
    FlySwattingSampleBank
do
    run_asar "ASSEMBLE $bank" \
        asar \
        --no-title-check \
        --define GameID=MPAINT \
        --define ROMID="$ROM_ID" \
        --define FileType=4 \
        --define PathToFile="SPC700/$bank.asm" \
        ../Global/AssembleFile.asm \
        "SPC700/$bank.bin" || exit 1
done

run_asar "ASSEMBLE SNES ROM" \
    asar \
    --define GameID=MPAINT \
    --define ROMID="$ROM_ID" \
    --define FileType=1 \
    ../Global/AssembleFile.asm \
    "$OUTPUT_PATH" || exit 1

run_asar "QUERY FIRMWARE" \
    asar \
    --define GameID=MPAINT \
    --define ROMID="$ROM_ID" \
    --define FileType=6 \
    ../Global/AssembleFile.asm \
    "$TEMP_FILE" || exit 1

FIRMWARE="$(tr -d '\r\n' < "$TEMP_FILE" 2>/dev/null)"
if [[ -n "$FIRMWARE" && "$FIRMWARE" != "NULL" ]]; then
    if [[ -f "$SCRIPT_DIR/$FIRMWARE" ]]; then
        :
    elif [[ -f "$SCRIPT_DIR/../Firmware/$FIRMWARE" ]]; then
        cp -- "$SCRIPT_DIR/../Firmware/$FIRMWARE" "$SCRIPT_DIR/$FIRMWARE" || fail "could not copy required firmware"
    else
        fail "required firmware not found: $FIRMWARE"
    fi
fi

run_asar "FINALIZE ROM" \
    asar \
    --define GameID=MPAINT \
    --define ROMID="$ROM_ID" \
    --define FileType=2 \
    ../Global/AssembleFile.asm \
    "$OUTPUT_PATH" || exit 1

run_asar "DISPLAY FINAL CHECKSUM" \
    asar \
    --fix-checksum=off \
    --define GameID=MPAINT \
    --define ROMID="$ROM_ID" \
    --define FileType=3 \
    ../Global/AssembleFile.asm \
    "$OUTPUT_PATH" || exit 1

echo
echo "===== BUILD RESULT ====="
stat -c '%n %s bytes' "$OUTPUT_PATH" 2>/dev/null || ls -l -- "$OUTPUT_PATH"
command -v md5sum >/dev/null 2>&1 && md5sum "$OUTPUT_PATH"
command -v sha256sum >/dev/null 2>&1 && sha256sum "$OUTPUT_PATH"

echo
echo "Build completed: $OUTPUT_PATH"
