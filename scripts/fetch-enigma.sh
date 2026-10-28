#!/usr/bin/env bash
# Reconstruct complete Enigma patched Linux-NX sources from an immutable upstream commit.
set -euo pipefail

REV="${1:-r20}"
case "$REV" in r20|r21) ;; *) echo "Usage: $0 [r20|r21]" >&2; exit 2;; esac
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
UPSTREAM="${ENIGMA_UPSTREAM:-https://gitlab.com/l4t-community/kernel/linux-nx.git}"
BASE=80f1e68e163ac77d688f0244b03cf05f791b4781
DST="${ENIGMA_WORKTREE:-$ROOT/work/linux-enigma}"

if [[ -e "$DST" ]]; then
  echo "Refusing to overwrite existing directory: $DST" >&2
  exit 3
fi
mkdir -p "$(dirname "$DST")"
git clone --filter=blob:none "$UPSTREAM" "$DST"
git -C "$DST" checkout --detach "$BASE"
test "$(git -C "$DST" rev-parse HEAD)" = "$BASE"
for patch in "$ROOT/patches/enigma-r20-base.patch" "$ROOT/patches/enigma-r20-ltssm.patch"; do
  echo "Applying ${patch##*/}"
  git -C "$DST" apply --check "$patch"
  git -C "$DST" apply "$patch"
done
if [[ "$REV" == r21 ]]; then
  echo "Applying UNTESTED hardware hypothesis: R21 REFCLK"
  patch="$ROOT/patches/experimental/r21-pad-refclk.patch"
  git -C "$DST" apply --check "$patch"
  git -C "$DST" apply "$patch"
fi

echo "Enigma $REV kernel sources reconstructed at $DST"
echo "Source files modified:"
git -C "$DST" diff --stat
echo "WARNING: Source reconstruction is not a boot-test or a validated release."
