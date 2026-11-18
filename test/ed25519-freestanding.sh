#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=$(mktemp -d "${TMPDIR:-/tmp}/ed25519-freestanding.XXXXXX")
trap 'rm -rf "$tmp"' EXIT
for module in monocypher monocypher-ed25519; do
    "${CC:-cc}" -std=c99 -ffreestanding -fno-builtin -I"$root/include" \
        -c "$root/src/libsec/port/$module.c" -o "$tmp/$module.o"
done
"${LD:-ld}" -r "$tmp/monocypher.o" "$tmp/monocypher-ed25519.o" -o "$tmp/crypto.o"
undefined=$("${NM:-nm}" -u "$tmp/crypto.o")
if [ -n "$undefined" ]; then
    printf 'Unexpected dependencies:\n%s\n' "$undefined" >&2
    exit 1
fi
printf 'Ed25519 freestanding objects have no unresolved dependencies\n'
