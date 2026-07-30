#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
log=$(mktemp "${TMPDIR:-/tmp}/factotum-integration.XXXXXX")
service=factotum-test-$$
pid=

killservice()
{
	sig=$1
	ps -eo pid=,args= |
	awk -v service="$service" '
		index($0, service) &&
		(index($0, "factotum -n -s") || index($0, "9pserve -u")) {
			print $1
		}' |
	while IFS= read -r p; do
		[ -n "$p" ] || continue
		[ "$p" != "$$" ] || continue
		kill "-$sig" "$p" >/dev/null 2>&1 || true
	done
}

removeservice()
{
	ns=${NAMESPACE:-}
	display=${DISPLAY:-}

	if [ -z "$ns" ] && [ -n "$display" ]; then
		case "$display" in
		*.0)
			display=${display%".0"}
			;;
		esac
		ns="/tmp/ns.$(id -un).$display"
	fi
	if [ -n "$ns" ]; then
		rm -f "$ns/$service" 2>/dev/null || true
	fi
}

cleanup()
{
	if [ -n "${pid:-}" ]; then
		kill "$pid" >/dev/null 2>&1 || true
		wait "$pid" >/dev/null 2>&1 || true
	fi
	if [ -n "${FACTOTUM_TEST_EXIT:-}" ]; then
		sleep 2
	fi
	killservice TERM
	sleep 1
	killservice KILL
	removeservice
	rm -f "$log"
}
trap cleanup EXIT

if [ ! -x "$root/factotum" ]; then
	make -C "$root" factotum
fi
if [ ! -x "$root/factotum-smoke-test" ]; then
	make -C "$root" factotum-smoke-test
fi

FACTOTUM_TPM_BACKEND=${FACTOTUM_TPM_BACKEND:-mock} "$root/factotum" -n -s "$service" >"$log" 2>&1 &
pid=$!

"$root/factotum-smoke-test" "$service" || {
	status=$?
	sed -n '1,120p' "$log"
	exit "$status"
}
