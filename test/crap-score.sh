#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
targets=${CRAP_TARGETS:-"src/cmd/auth/factotum/monokey.c src/cmd/auth/factotum/monocypherproto.c src/cmd/auth/factotum/tpm9p.c src/cmd/auth/factotum/oauth.c"}

if ! command -v gcov >/dev/null 2>&1; then
	echo "SKIP: gcov not found; CRAP score not run"
	exit 0
fi
if ! command -v python3 >/dev/null 2>&1; then
	echo "SKIP: python3 not found; CRAP score not run"
	exit 0
fi

tmp=$(mktemp -d "${TMPDIR:-/tmp}/factotum-crap.XXXXXX")
trap 'rm -rf "$tmp"' EXIT
rsync -a --exclude .git "$root/" "$tmp/"

covcflags="-Wall -O0 -g -Iinclude -Isrc/lib9 -Isrc/libthread -DPLAN9PORT -std=gnu11 -fno-common -D_GNU_SOURCE -DFACTOTUM_TEST_EXIT --coverage"
covldflags="--coverage -lpthread -lutil -lresolv"

make -C "$tmp" clean >/dev/null
FACTOTUM_TEST_EXIT=1 make -C "$tmp" CFLAGS="$covcflags" LDFLAGS="$covldflags" check check-integration

jsondir="$tmp/crap-gcov"
mkdir -p "$jsondir"
for src in $targets; do
	(
		cd "$jsondir"
		gcov -j -o "$tmp/$(dirname "$src")" "$tmp/$src" >/dev/null
	)
done

python3 "$root/test/crap-score.py" \
	--root "$tmp" \
	--json-dir "$jsondir" \
	--threshold "${CRAP_MAX:-30}" \
	$targets
