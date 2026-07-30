#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=$(mktemp -d "${TMPDIR:-/tmp}/factotum-asan.XXXXXX")
trap 'rm -rf "$tmp"' EXIT

rsync -a --exclude .git "$root/" "$tmp/"

asan_cflags="-Wall -O1 -g -Iinclude -Isrc/lib9 -Isrc/libthread -DPLAN9PORT -std=gnu11 -fno-common -fno-omit-frame-pointer -fsanitize=address,undefined"
asan_ldflags="-fsanitize=address,undefined -lpthread"
if [ "$(uname)" = Linux ]; then
	asan_cflags="$asan_cflags -D_GNU_SOURCE"
	asan_ldflags="$asan_ldflags -lutil -lresolv"
fi

ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=0:abort_on_error=1}
UBSAN_OPTIONS=${UBSAN_OPTIONS:-halt_on_error=1}
export ASAN_OPTIONS UBSAN_OPTIONS

make -C "$tmp" clean >/dev/null 2>&1 || true
make -C "$tmp" -j"${JOBS:-4}" CFLAGS="$asan_cflags" LDFLAGS="$asan_ldflags" all check
