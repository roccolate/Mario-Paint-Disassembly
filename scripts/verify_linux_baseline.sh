#!/usr/bin/env bash

# End-to-end Linux baseline validation in an isolated temporary worktree.
# Usage: scripts/verify_linux_baseline.sh /path/to/Mario\ Paint\ \(Japan,\ USA\).sfc

if [[ "${BASH_SOURCE[0]}" != "$0" ]]; then
    echo "ERROR: run this script instead of sourcing it." >&2
    return 2
fi

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(git -C "$SCRIPT_DIR" rev-parse --show-toplevel 2>/dev/null)"
ORIGINAL_ROM="${1:-}"
EXPECTED_MD5="881d3772a3eb37a8a0fb254e940c6767"
EXPECTED_SHA256="e842cac1a4301be196f1e137fbd1a16866d5c913f24dbca313f4dd8bd7472f45"

fail() {
    echo "ERROR: $*" >&2
    exit 1
}

[[ -n "$REPO_ROOT" ]] || fail "could not locate the Git repository"
[[ -n "$ORIGINAL_ROM" ]] || fail "usage: $0 /path/to/Mario-Paint-JU.sfc"
[[ -f "$ORIGINAL_ROM" ]] || fail "ROM not found: $ORIGINAL_ROM"
command -v git >/dev/null 2>&1 || fail "git was not found in PATH"
command -v asar >/dev/null 2>&1 || fail "asar was not found in PATH"
command -v python3 >/dev/null 2>&1 || fail "python3 was not found in PATH"
command -v md5sum >/dev/null 2>&1 || fail "md5sum was not found in PATH"
command -v sha256sum >/dev/null 2>&1 || fail "sha256sum was not found in PATH"
command -v cmp >/dev/null 2>&1 || fail "cmp was not found in PATH"

ACTUAL_MD5="$(md5sum "$ORIGINAL_ROM" | awk '{print $1}')"
ACTUAL_SHA256="$(sha256sum "$ORIGINAL_ROM" | awk '{print $1}')"
[[ "$ACTUAL_MD5" == "$EXPECTED_MD5" ]] || fail "unexpected ROM MD5: $ACTUAL_MD5"
[[ "$ACTUAL_SHA256" == "$EXPECTED_SHA256" ]] || fail "unexpected ROM SHA256: $ACTUAL_SHA256"

HEAD_SHA="$(git -C "$REPO_ROOT" rev-parse HEAD)" || fail "could not resolve HEAD"
WORK_PARENT="$(mktemp -d "${TMPDIR:-/tmp}/mpaint-baseline.XXXXXX")" || fail "could not create temporary directory"
WORKTREE="$WORK_PARENT/tree"

cleanup() {
    if [[ "${KEEP_WORKTREE:-0}" == "1" ]]; then
        echo "Temporary worktree retained: $WORKTREE"
        return
    fi
    git -C "$REPO_ROOT" worktree remove --force "$WORKTREE" >/dev/null 2>&1 || true
    rm -rf -- "$WORK_PARENT"
}
trap cleanup EXIT

echo "===== MARIO PAINT LINUX BASELINE ====="
echo "Commit: $HEAD_SHA"
echo "ROM: $ORIGINAL_ROM"
echo "Asar: $(asar --version 2>&1 | head -1)"

echo
echo "===== CREATE ISOLATED WORKTREE ====="
if ! git -C "$REPO_ROOT" worktree add --detach "$WORKTREE" "$HEAD_SHA"; then
    fail "could not create temporary worktree"
fi

echo
echo "===== EXTRACT ====="
if ! bash "$WORKTREE/MPAINT/AsarScripts/ExtractAssets.sh" "$ORIGINAL_ROM" MPAINT_JU; then
    fail "asset extraction failed"
fi

REBUILT_ROM="$WORKTREE/MPAINT/Mario Paint (JU).sfc"

echo
echo "===== BUILD ====="
if ! bash "$WORKTREE/MPAINT/Assemble_MPAINT.sh" MPAINT_JU "$REBUILT_ROM"; then
    fail "ROM build failed"
fi

[[ -f "$REBUILT_ROM" ]] || fail "rebuilt ROM was not produced"

echo
echo "===== COMPARE ====="
md5sum "$ORIGINAL_ROM" "$REBUILT_ROM"
sha256sum "$ORIGINAL_ROM" "$REBUILT_ROM"

REBUILT_MD5="$(md5sum "$REBUILT_ROM" | awk '{print $1}')"
REBUILT_SHA256="$(sha256sum "$REBUILT_ROM" | awk '{print $1}')"

[[ "$REBUILT_MD5" == "$EXPECTED_MD5" ]] || fail "rebuilt MD5 mismatch: $REBUILT_MD5"
[[ "$REBUILT_SHA256" == "$EXPECTED_SHA256" ]] || fail "rebuilt SHA256 mismatch: $REBUILT_SHA256"

if ! cmp -s "$ORIGINAL_ROM" "$REBUILT_ROM"; then
    echo "First differing byte positions:" >&2
    cmp -l "$ORIGINAL_ROM" "$REBUILT_ROM" | head -20 >&2 || true
    fail "rebuilt ROM is not byte-identical to the original"
fi

echo
echo "BIT-PERFECT: rebuilt ROM is identical to the verified original"
echo "Baseline validation passed for commit $HEAD_SHA"
