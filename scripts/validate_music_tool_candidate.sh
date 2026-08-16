#!/usr/bin/env bash

if [[ "${BASH_SOURCE[0]}" != "$0" ]]; then
    echo "ERROR: run this script instead of sourcing it." >&2
    return 2
fi

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/.." && pwd)"
MUSIC_TOOL="$REPO_ROOT/tools/mpaint-music/mpaint-music"
SAVE_TOOL="$REPO_ROOT/tools/mpaint-save/mpaint-save"

fail() {
    echo "ERROR: $*" >&2
    exit 1
}

run_music_file() {
    local path="$1"
    local size="$2"
    local command=""

    case "$size" in
        592)
            command="validate"
            ;;
        47698)
            command="validate-composition"
            ;;
        *)
            fail "unsupported input size for $path: $size bytes"
            ;;
    esac

    echo
    echo "===== MUSIC TOOL INPUT: $path ====="
    stat -c '%n %s bytes' "$path"

    "$MUSIC_TOOL" "$command" "$path" || fail "Music Tool mapped-format validation failed: $path"

    if [[ "$command" == "validate" ]]; then
        "$MUSIC_TOOL" inspect "$path" || fail "Music Tool inspection failed: $path"
        if [[ "${SHOW_EVENTS:-0}" == "1" ]]; then
            "$MUSIC_TOOL" events "$path" || fail "Music Tool event dump failed: $path"
        fi
    else
        "$MUSIC_TOOL" inspect-composition "$path" || fail "Music Tool composition inspection failed: $path"
        if [[ "${SHOW_EVENTS:-0}" == "1" ]]; then
            "$MUSIC_TOOL" events-composition "$path" || fail "Music Tool composition event dump failed: $path"
        fi
    fi
}

echo '===== MUSIC TOOL: CLEAN BUILD + TESTS ====='
make -C "$REPO_ROOT/tools/mpaint-music" clean || fail "music-tool clean failed"
make -C "$REPO_ROOT/tools/mpaint-music" test || fail "music-tool tests failed"
make -C "$REPO_ROOT/tools/mpaint-music" all || fail "music-tool build failed"

echo
echo 'Synthetic Music Tool validation passed.'

if [[ "$#" -eq 0 ]]; then
    echo 'Provide one or more music.bin (592 bytes), composition.bin (47698 bytes), or Mario Paint .srm (32768 bytes) files to run data gates.'
    exit 0
fi

for path in "$@"; do
    [[ -f "$path" ]] || fail "input not found: $path"
    size="$(stat -c '%s' "$path")" || fail "could not stat: $path"

    if [[ "$size" == "32768" ]]; then
        echo
        echo "===== REAL SRAM INPUT: $path ====="
        if [[ ! -x "$SAVE_TOOL" ]]; then
            make -C "$REPO_ROOT/tools/mpaint-save" all || fail "could not build save decoder"
        fi
        tmp="$(mktemp -d "${TMPDIR:-/tmp}/mpaint-music-srm.XXXXXX")" || fail "could not create temp directory"
        if ! "$SAVE_TOOL" decode "$path" "$tmp/project"; then
            rm -rf -- "$tmp"
            fail "could not decode Mario Paint SRAM: $path"
        fi
        run_music_file "$tmp/project/music.bin" 592
        rm -rf -- "$tmp"
    else
        run_music_file "$path" "$size"
    fi
done

echo
echo 'MUSIC TOOL DATA GATES PASSED'
