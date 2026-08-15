#!/usr/bin/env bash

if [[ "${BASH_SOURCE[0]}" != "$0" ]]; then
    echo "ERROR: run this script instead of sourcing it." >&2
    return 2
fi

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/.." && pwd)"
TOOL_DIR="$REPO_ROOT/tools/mpaint-save"
ROM_PATH="${1:-}"
EXPECTED_MD5="881d3772a3eb37a8a0fb254e940c6767"

fail() {
    echo "ERROR: $*" >&2
    exit 1
}

[[ -n "$ROM_PATH" ]] || fail "usage: bash scripts/run_mesen_real_save_gate.sh /path/to/Mario\ Paint\ \(Japan,\ USA\).sfc"
[[ -f "$ROM_PATH" ]] || fail "ROM not found: $ROM_PATH"
command -v md5sum >/dev/null 2>&1 || fail "md5sum was not found"
command -v find >/dev/null 2>&1 || fail "find was not found"
command -v sync >/dev/null 2>&1 || fail "sync was not found"

if [[ -n "${MPAINT_MESEN2:-}" ]]; then
    MESEN_BIN="$MPAINT_MESEN2"
elif command -v mesen2 >/dev/null 2>&1; then
    MESEN_BIN="$(command -v mesen2)"
else
    fail "mesen2 was not found; set MPAINT_MESEN2=/path/to/mesen2"
fi

ROM_MD5="$(md5sum "$ROM_PATH" | awk '{print $1}')"
[[ "$ROM_MD5" == "$EXPECTED_MD5" ]] || fail "unexpected ROM MD5: $ROM_MD5"

if [[ ! -x "$TOOL_DIR/mpaint-save" || ! -x "$TOOL_DIR/mpaint-save-rebuild" ]]; then
    echo '===== BUILD SAVE CODEC ====='
    make -C "$TOOL_DIR" all || fail "could not build save codec"
fi

MARKER="$(mktemp "${TMPDIR:-/tmp}/mpaint-mesen-marker.XXXXXX")" || fail "could not create marker"
trap 'rm -f -- "$MARKER"' EXIT

touch "$MARKER"

SAVE_ROOTS=(
    "$HOME/.config/Mesen2/Saves"
    "$HOME/.config/Mesen2"
    "$HOME/.local/share/Mesen2/Saves"
    "$HOME/.local/share/mesen2/Saves"
    "$(dirname -- "$ROM_PATH")"
)

echo '===== MESEN REAL-SAVE GATE ====='
echo "Mesen: $MESEN_BIN"
echo "ROM:   $ROM_PATH"
echo
echo 'Mesen will open now.'
echo '1. If the Mario Paint cursor does not move, configure SNES controller port 2 as Mouse.'
echo '2. Enter the drawing screen and make one visible mark so this is not an untouched save.'
echo "3. Open Mario Paint's own Save/Load screen and perform an in-game SAVE."
echo '4. Return to the drawing screen, then close Mesen completely.'
echo 'After Mesen closes, this script will locate the changed 32 KiB SRAM automatically.'
echo

"$MESEN_BIN" "$ROM_PATH"
MESEN_RC=$?
echo
echo "Mesen exit code: $MESEN_RC"

sync
sleep 1

CANDIDATES_FILE="$(mktemp "${TMPDIR:-/tmp}/mpaint-mesen-candidates.XXXXXX")" || fail "could not create candidate list"
trap 'rm -f -- "$MARKER" "$CANDIDATES_FILE"' EXIT
: > "$CANDIDATES_FILE"

for root in "${SAVE_ROOTS[@]}"; do
    [[ -d "$root" ]] || continue
    find "$root" -maxdepth 4 -type f -size 32768c -newer "$MARKER" -print 2>/dev/null >> "$CANDIDATES_FILE"
done

sort -u -o "$CANDIDATES_FILE" "$CANDIDATES_FILE"

echo
echo '===== CHANGED 32 KiB SAVE CANDIDATES ====='
if [[ ! -s "$CANDIDATES_FILE" ]]; then
    echo 'No changed 32 KiB files were found.'
    echo 'Search roots were:'
    printf '  %s\n' "${SAVE_ROOTS[@]}"
    exit 3
fi
cat "$CANDIDATES_FILE"

VALID_COUNT=0
VALID_SAVE=""

while IFS= read -r candidate; do
    [[ -n "$candidate" ]] || continue
    echo
    echo "===== PROBE: $candidate ====="
    if "$TOOL_DIR/mpaint-save" inspect "$candidate"; then
        VALID_COUNT=$((VALID_COUNT + 1))
        VALID_SAVE="$candidate"
        echo 'Candidate decodes as Mario Paint SRAM.'
    else
        echo 'Rejected: not a valid Mario Paint composition save.'
    fi
done < "$CANDIDATES_FILE"

if [[ "$VALID_COUNT" -eq 0 ]]; then
    fail "changed 32 KiB files were found, but none passed the Mario Paint decoder"
fi
if [[ "$VALID_COUNT" -ne 1 ]]; then
    fail "more than one valid Mario Paint save candidate was found; inspect the paths above"
fi

echo
echo '===== FULL REAL-SAVE ROUND-TRIP ====='
echo "Using: $VALID_SAVE"
KEEP_OUTPUT=1 bash "$REPO_ROOT/scripts/validate_save_codec_candidate.sh" "$VALID_SAVE" || fail "real-save round-trip failed"

echo
echo 'MESEN REAL-SAVE COMPATIBILITY GATE PASSED'
echo "Validated save: $VALID_SAVE"
