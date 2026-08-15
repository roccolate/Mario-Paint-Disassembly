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
command -v sort >/dev/null 2>&1 || fail "sort was not found"

if [[ -n "${MPAINT_MESEN2:-}" ]]; then
    MESEN_BIN="$MPAINT_MESEN2"
elif command -v mesen2 >/dev/null 2>&1; then
    MESEN_BIN="$(command -v mesen2)"
else
    fail "Mesen was not found; set MPAINT_MESEN2=/path/to/Mesen"
fi

[[ -x "$MESEN_BIN" ]] || fail "Mesen executable is not executable: $MESEN_BIN"

ROM_MD5="$(md5sum "$ROM_PATH" | awk '{print $1}')"
[[ "$ROM_MD5" == "$EXPECTED_MD5" ]] || fail "unexpected ROM MD5: $ROM_MD5"

if [[ ! -x "$TOOL_DIR/mpaint-save" || ! -x "$TOOL_DIR/mpaint-save-rebuild" ]]; then
    echo '===== BUILD SAVE CODEC ====='
    make -C "$TOOL_DIR" all || fail "could not build save codec"
fi

MESENCE_HOME="$HOME/.config/MesenCE"
MESENCE_SETTINGS="$MESENCE_HOME/settings.json"

# MesenCE's first-run wizard restarts Mesen without preserving the original ROM
# argument. Refuse that ambiguous state so the compatibility gate cannot report a
# false "no SRAM" result. Complete MesenCE's initial setup once, then rerun.
if [[ "$MESEN_BIN" == *Mesen* && "$MESEN_BIN" == *mesence* && ! -f "$MESENCE_SETTINGS" ]]; then
    echo '===== MESENCE FIRST-RUN SETUP REQUIRED ====='
    echo "Missing: $MESENCE_SETTINGS"
    echo 'Run MesenCE once without a ROM, complete its initial setup, close it completely,'
    echo 'then rerun this gate with the same HOME/XDG environment.'
    exit 4
fi

MARKER="$(mktemp "${TMPDIR:-/tmp}/mpaint-mesen-marker.XXXXXX")" || fail "could not create marker"
CANDIDATES_FILE="$(mktemp "${TMPDIR:-/tmp}/mpaint-mesen-candidates.XXXXXX")" || fail "could not create candidate list"
CHANGED_FILE="$(mktemp "${TMPDIR:-/tmp}/mpaint-mesen-changed.XXXXXX")" || fail "could not create changed-file list"
trap 'rm -f -- "$MARKER" "$CANDIDATES_FILE" "$CHANGED_FILE"' EXIT

touch "$MARKER"
: > "$CANDIDATES_FILE"
: > "$CHANGED_FILE"

SAVE_ROOTS=(
    "$HOME/.config/MesenCE"
    "$HOME/.config/Mesen2"
    "$HOME/.local/share/MesenCE"
    "$HOME/.local/share/mesen2"
    "$(dirname -- "$ROM_PATH")"
    "$(dirname -- "$MESEN_BIN")"
)

echo '===== MESEN REAL-SAVE GATE ====='
echo "Mesen: $MESEN_BIN"
echo "ROM:   $ROM_PATH"
if [[ -f "$MESENCE_SETTINGS" ]]; then
    echo "MesenCE home: $MESENCE_HOME"
fi
echo
echo 'Mesen will open now.'
echo '1. Confirm Mario Paint itself is visible and running before continuing.'
echo '2. If the Mario Paint cursor does not move, configure SNES controller port 2 as Mouse.'
echo '3. Enter the drawing screen and make one visible mark so this is not an untouched save.'
echo "4. Open Mario Paint's own Save/Load screen and perform an in-game SAVE."
echo '5. Return to the drawing screen, then close Mesen completely.'
echo 'After Mesen closes, this script will locate the changed SRAM automatically.'
echo

"$MESEN_BIN" "$ROM_PATH"
MESEN_RC=$?
echo
echo "Mesen exit code: $MESEN_RC"

sync
sleep 1

for root in "${SAVE_ROOTS[@]}"; do
    [[ -d "$root" ]] || continue

    find "$root" -maxdepth 6 -type f -newer "$MARKER" \
        -printf '%s\t%p\n' 2>/dev/null >> "$CHANGED_FILE"

    find "$root" -maxdepth 6 -type f -size 32768c -newer "$MARKER" \
        -print 2>/dev/null >> "$CANDIDATES_FILE"
done

sort -u -o "$CHANGED_FILE" "$CHANGED_FILE"
sort -u -o "$CANDIDATES_FILE" "$CANDIDATES_FILE"

echo
echo '===== FILES CHANGED DURING MESEN SESSION ====='
if [[ -s "$CHANGED_FILE" ]]; then
    cat "$CHANGED_FILE"
else
    echo 'No changed files were found in the monitored roots.'
fi

echo
echo '===== CHANGED 32 KiB SAVE CANDIDATES ====='
if [[ ! -s "$CANDIDATES_FILE" ]]; then
    echo 'No changed 32 KiB files were found.'

    if [[ -f "$MESENCE_SETTINGS" && ! -d "$MESENCE_HOME/Saves" ]]; then
        echo
        echo "MesenCE did not create $MESENCE_HOME/Saves."
        echo 'For a SNES cartridge with SRAM, that normally means the ROM did not reach'
        echo 'the cartridge SRAM initialization path during this session.'
    fi

    echo
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
