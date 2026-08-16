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
    local rc=0

    case "$size" in
        592)
            command="validate"
            ;;
        47698)
            command="validate-composition"
            ;;
        *)
            echo "ERROR: unsupported input size for $path: $size bytes" >&2
            return 1
            ;;
    esac

    echo
    echo "===== MUSIC TOOL INPUT: $path ====="
    stat -c '%n %s bytes' "$path" || return 1

    if ! "$MUSIC_TOOL" "$command" "$path"; then
        echo "ERROR: Music Tool mapped-format validation failed: $path" >&2
        rc=1
    fi

    if [[ "$command" == "validate" ]]; then
        "$MUSIC_TOOL" inspect "$path" || rc=1
        if [[ "${SHOW_EVENTS:-0}" == "1" ]]; then
            "$MUSIC_TOOL" events "$path" || rc=1
        fi
    else
        "$MUSIC_TOOL" inspect-composition "$path" || rc=1
        if [[ "${SHOW_EVENTS:-0}" == "1" ]]; then
            "$MUSIC_TOOL" events-composition "$path" || rc=1
        fi
    fi

    return "$rc"
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

FAILURES=0

for path in "$@"; do
    if [[ ! -f "$path" ]]; then
        echo "ERROR: input not found: $path" >&2
        FAILURES=$((FAILURES + 1))
        continue
    fi

    size="$(stat -c '%s' "$path")" || {
        echo "ERROR: could not stat: $path" >&2
        FAILURES=$((FAILURES + 1))
        continue
    }

    if [[ "$size" == "32768" ]]; then
        echo
        echo "===== REAL SRAM INPUT: $path ====="
        if [[ ! -x "$SAVE_TOOL" ]]; then
            make -C "$REPO_ROOT/tools/mpaint-save" all || fail "could not build save decoder"
        fi
        tmp="$(mktemp -d "${TMPDIR:-/tmp}/mpaint-music-srm.XXXXXX")" || fail "could not create temp directory"
        if ! "$SAVE_TOOL" decode "$path" "$tmp/project"; then
            echo "ERROR: could not decode Mario Paint SRAM: $path" >&2
            rm -rf -- "$tmp"
            FAILURES=$((FAILURES + 1))
            continue
        fi
        if ! run_music_file "$tmp/project/music.bin" 592; then
            FAILURES=$((FAILURES + 1))
        fi
        rm -rf -- "$tmp"
    else
        if ! run_music_file "$path" "$size"; then
            FAILURES=$((FAILURES + 1))
        fi
    fi
done

echo
if [[ "$FAILURES" -ne 0 ]]; then
    echo "MUSIC TOOL DATA GATES: $FAILURES input(s) failed"
    exit 1
fi

echo 'MUSIC TOOL DATA GATES PASSED'
