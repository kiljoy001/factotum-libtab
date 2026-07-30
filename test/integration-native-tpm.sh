#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
state=$(mktemp -d "${TMPDIR:-/tmp}/factotum-swtpm-state.XXXXXX")
pidfile=$(mktemp "${TMPDIR:-/tmp}/factotum-swtpm-pid.XXXXXX")
log=$(mktemp "${TMPDIR:-/tmp}/factotum-swtpm.XXXXXX")
base=$((22000 + ($$ % 10000)))
server_port=$base
ctrl_port=$((base + 1))
key_handle=$(printf "0x81%06x" $((0x110000 + ($$ % 0x0f0000))))
vault_handle=$(printf "0x81%06x" $((0x100000 + ($$ % 0x0f0000))))

cleanup()
{
	if [ -s "$pidfile" ]; then
		kill "$(cat "$pidfile")" >/dev/null 2>&1 || true
	fi
	rm -f "$pidfile" "$log"
	rm -rf "$state"
}
trap cleanup EXIT

if ! command -v swtpm >/dev/null 2>&1; then
	echo "swtpm not found" >&2
	exit 1
fi

if [ ! -x "$root/factotum" ] || [ ! -x "$root/factotum-smoke-test" ]; then
	make -C "$root" factotum factotum-smoke-test
fi

swtpm socket --tpm2 \
	--tpmstate "dir=$state" \
	--ctrl "type=tcp,port=$ctrl_port" \
	--server "type=tcp,port=$server_port" \
	--flags not-need-init,startup-clear \
	--pid "file=$pidfile" \
	--daemon \
	--log "file=$log,level=1"

i=0
while [ "$i" -lt 50 ]; do
	if [ -s "$pidfile" ]; then
		break
	fi
	i=$((i + 1))
	sleep 0.1
done
if [ ! -s "$pidfile" ]; then
	echo "swtpm did not start" >&2
	sed -n '1,120p' "$log" >&2 || true
	exit 1
fi

prefix=${TPM2_TSS_PREFIX:-"$HOME/.local/tpm2-tss"}
pkg_arch=$(${CC:-gcc} -dumpmachine 2>/dev/null || true)
local_lib_dirs="
$prefix/usr/lib/$pkg_arch
$prefix/usr/lib/x86_64-linux-gnu
$prefix/usr/lib64
$prefix/usr/lib
$prefix/lib/$pkg_arch
$prefix/lib/x86_64-linux-gnu
$prefix/lib64
$prefix/lib
"
local_ld_path=
for dir in $local_lib_dirs; do
	if [ -z "$pkg_arch" ] &&
		{ [ "$dir" = "$prefix/usr/lib/" ] || [ "$dir" = "$prefix/lib/" ]; }; then
		continue
	fi
	if [ -d "$dir" ]; then
		if [ -n "$local_ld_path" ]; then
			local_ld_path="$local_ld_path:$dir"
		else
			local_ld_path="$dir"
		fi
	fi
done
if [ -n "$local_ld_path" ]; then
	export LD_LIBRARY_PATH="$local_ld_path${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi

FACTOTUM_TPM_BACKEND=tpm2 \
FACTOTUM_TPM_TCTI="swtpm:host=127.0.0.1,port=$server_port" \
FACTOTUM_TPM_QUOTE_HANDLE="$key_handle" \
FACTOTUM_TEST_NATIVE_TPM=1 \
FACTOTUM_TEST_TPM_KEY_HANDLE="$key_handle" \
FACTOTUM_TEST_TPM_VAULT_HANDLE="$vault_handle" \
sh "$root/test/integration-factotum.sh"
