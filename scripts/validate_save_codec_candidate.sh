#!/usr/bin/env bash

if [[ "${BASH_SOURCE[0]}" != "$0" ]]; then
    echo "ERROR: run this script instead of sourcing it." >&2
    return 2
fi

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/.." && pwd)"
TOOL_DIR="$REPO_ROOT/tools/mpaint-save"
SAVE_PATH="${1:-}"

fail() {
    echo "ERROR: $*" >&2
    exit 1
}

command -v make >/dev/null 2>&1 || fail "make was not found in PATH"
command -v cmp >/dev/null 2>&1 || fail "cmp was not found in PATH"

if command -v cc >/dev/null 2>&1; then
    CC_BIN=cc
elif command -v gcc >/dev/null 2>&1; then
    CC_BIN=gcc
else
    fail "no C compiler was found"
fi

printf '%s\n' '===== SAVE CODEC: CLEAN BUILD + SYNTHETIC TESTS ====='
make -C "$TOOL_DIR" CC="$CC_BIN" clean || fail "clean failed"
make -C "$TOOL_DIR" CC="$CC_BIN" test || fail "synthetic codec tests failed"
make -C "$TOOL_DIR" CC="$CC_BIN" all || fail "codec build failed"

if [[ -z "$SAVE_PATH" ]]; then
    echo
    echo "Synthetic decode/encode validation passed."
    echo "Provide a 32 KiB Mario Paint .srm as argument to run the real-save compatibility gate."
    exit 0
fi

[[ -f "$SAVE_PATH" ]] || fail "save not found: $SAVE_PATH"

printf '\n%s\n' '===== SAVE CODEC: REAL SAVE INSPECTION ====='
"$TOOL_DIR/mpaint-save" inspect "$SAVE_PATH" || fail "real save inspection failed"

WORK_DIR="$(mktemp -d "${TMPDIR:-/tmp}/mpaint-save-codec.XXXXXX")" || fail "could not create temporary directory"
PROJECT_DIR="$WORK_DIR/project"
REBUILT_SAVE="$WORK_DIR/rebuilt.srm"
REBUILT_PROJECT="$WORK_DIR/rebuilt-project"
cleanup() {
    if [[ "${KEEP_OUTPUT:-0}" == "1" ]]; then
        echo "Validation output retained: $WORK_DIR"
        return
    fi
    rm -rf -- "$WORK_DIR"
}
trap cleanup EXIT

printf '\n%s\n' '===== SAVE CODEC: REAL SAVE DECODE ====='
"$TOOL_DIR/mpaint-save" decode "$SAVE_PATH" "$PROJECT_DIR" || fail "real save decode failed"

[[ "$(wc -c < "$PROJECT_DIR/composition.bin")" -eq 47698 ]] || fail "unexpected composition size"
[[ "$(wc -c < "$PROJECT_DIR/animation.bin")" -eq 22528 ]] || fail "unexpected animation size"
[[ "$(wc -c < "$PROJECT_DIR/animation-path.bin")" -eq 2048 ]] || fail "unexpected animation-path size"
[[ "$(wc -c < "$PROJECT_DIR/canvas.bin")" -eq 22528 ]] || fail "unexpected canvas size"
[[ "$(wc -c < "$PROJECT_DIR/music.bin")" -eq 592 ]] || fail "unexpected music size"
[[ "$(wc -c < "$PROJECT_DIR/tail.bin")" -eq 2 ]] || fail "unexpected tail size"

cat \
    "$PROJECT_DIR/animation.bin" \
    "$PROJECT_DIR/animation-path.bin" \
    "$PROJECT_DIR/canvas.bin" \
    "$PROJECT_DIR/music.bin" \
    "$PROJECT_DIR/tail.bin" \
    > "$WORK_DIR/rejoined.bin"

cmp -s "$PROJECT_DIR/composition.bin" "$WORK_DIR/rejoined.bin" || fail "section split does not reconstruct composition.bin"

printf '\n%s\n' '===== SAVE CODEC: REBUILD FROM REAL TEMPLATE ====='
"$TOOL_DIR/mpaint-save-rebuild" \
    "$SAVE_PATH" \
    "$PROJECT_DIR/composition.bin" \
    "$REBUILT_SAVE" || fail "real save rebuild failed"

printf '\n%s\n' '===== SAVE CODEC: DECODE REBUILT SAVE ====='
"$TOOL_DIR/mpaint-save" decode "$REBUILT_SAVE" "$REBUILT_PROJECT" || fail "rebuilt save decode failed"

cmp -s \
    "$PROJECT_DIR/composition.bin" \
    "$REBUILT_PROJECT/composition.bin" || fail "rebuilt save changed the uncompressed composition"

echo
echo "REAL-SAVE CODEC ROUND-TRIP PASSED"
echo "original .srm -> composition -> rebuilt .srm -> composition is byte-identical"
echo "Compressed .srm bytes are not expected to be identical."
