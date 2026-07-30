#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
secs=${1:-10}

clang_candidates()
{
	if [ -n "${CLANG:-}" ]; then
		printf '%s\n' "$CLANG"
	fi
	for cc in clang clang-23 clang-18 clang-17 clang-16 clang-15 clang-14; do
		printf '%s\n' "$cc"
	done
}

tmp=$(mktemp -d "${TMPDIR:-/tmp}/factotum-fuzz.XXXXXX")
trap 'rm -rf "$tmp"' EXIT
mkdir -p "$tmp/corpus"
printf 'abcdefghijklmnopqrstuvwxyz0123456789' > "$tmp/corpus/seed"

cc=
tried=0
for candidate in $(clang_candidates); do
	if ! command -v "$candidate" >/dev/null 2>&1; then
		continue
	fi
	tried=1
	if "$candidate" -x c - -o "$tmp/fuzzer-probe" \
		-fsanitize=fuzzer,address,undefined >/dev/null 2>&1 <<'EOF'
int LLVMFuzzerTestOneInput(const unsigned char *data, unsigned long size)
{
	(void)data;
	(void)size;
	return 0;
}
EOF
	then
		cc=$candidate
		break
	fi
done

if [ -z "$cc" ]; then
	if [ "$tried" -eq 0 ]; then
		echo "SKIP: clang not found; libFuzzer target not run"
	else
		echo "SKIP: no clang could link libFuzzer/ASAN/UBSAN runtime; Monocypher fuzz target not run"
	fi
	exit 0
fi

"$cc" -g -O1 -std=gnu11 -I"$root/include" \
	-fsanitize=fuzzer,address,undefined \
	"$root/src/libsec/port/monocypher.c" \
	"$root/src/libsec/port/monocypherfuzz.c" \
	-o "$tmp/monocypher-fuzz"

"$tmp/monocypher-fuzz" "$tmp/corpus" -max_total_time="$secs" -print_final_stats=1
