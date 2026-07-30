#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=$(mktemp -d "${TMPDIR:-/tmp}/factotum-mutant.XXXXXX")
log=$(mktemp "${TMPDIR:-/tmp}/factotum-mutant-log.XXXXXX")
trap 'rm -rf "$tmp" "$log"' EXIT

rsync -a --exclude .git "$root/" "$tmp/"

perl -0pi -e 's/return crypto_eddsa_check_equation\(signature, public_key, h\);/return 0; \/* MUTANT: accept every signature *\// or die "mutation did not apply\n"' \
	"$tmp/src/libsec/port/monocypher.c"

set +e
make -C "$tmp" check-unit >"$log" 2>&1
rc=$?
set -e

if [ "$rc" -eq 0 ]; then
	echo "mutation survived: check-unit did not catch always-accept verifier"
	sed -n '1,160p' "$log"
	exit 1
fi

echo "mutation killed: always-accept verifier was caught"
