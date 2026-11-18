#!/usr/bin/env bash
# Coverage-guided fuzzing for the C# factotum harness via SharpFuzz and AFL.
#
# The xUnit fuzz harness runs on every build. This script is the deeper campaign
# for finding parser and framing states that fixed random seeds do not reach.
#
# Usage: FUZZ_SECONDS=30 tests/fuzz.sh [attr|rpc]

set -euo pipefail

TARGET="${1:-attr}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROJECT="$ROOT/tests/Factotum.Fuzz"
OUT="$PROJECT/bin/fuzz"
FUZZ_SECONDS="${FUZZ_SECONDS:-10}"

if ! command -v sharpfuzz >/dev/null 2>&1; then
  echo "SKIP: sharpfuzz is not on PATH"
  exit 0
fi

if ! command -v afl-fuzz >/dev/null 2>&1; then
  echo "SKIP: afl-fuzz is not on PATH"
  exit 0
fi

echo "building fuzz target: $TARGET"
rm -rf "$OUT"
dotnet publish "$PROJECT/Factotum.Fuzz.csproj" -c Release -o "$OUT"

echo "instrumenting assembly"
sharpfuzz "$OUT/Factotum.Testing.dll"

mkdir -p "$PROJECT/testcases" "$PROJECT/findings"
if [ -z "$(ls -A "$PROJECT/testcases" 2>/dev/null)" ]; then
  printf 'ok name=secstore policy=none blob=mock-v1:c2VjcmV0\n' > "$PROJECT/testcases/attr"
  printf 'start proto=tpmseal role=unseal blob=mock-v1:c2VjcmV0 !auth=login-secret' > "$PROJECT/testcases/rpc"
fi

RUN_ID="${TARGET}-$(date +%Y%m%d%H%M%S)-$$"
FINDINGS="$PROJECT/findings/$RUN_ID"
DOTNET_BIN="$(command -v dotnet)"

export AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES="${AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES:-1}"
export AFL_SKIP_BIN_CHECK="${AFL_SKIP_BIN_CHECK:-1}"
export AFL_SKIP_CPUFREQ="${AFL_SKIP_CPUFREQ:-1}"
export AFL_NO_UI="${AFL_NO_UI:-1}"

echo "starting afl-fuzz for ${FUZZ_SECONDS}s; findings land in $FINDINGS"
set +e
timeout "${FUZZ_SECONDS}s" afl-fuzz -i "$PROJECT/testcases" -o "$FINDINGS" -t 5000 -m none \
  -- "$DOTNET_BIN" "$OUT/Factotum.Fuzz.dll" "$TARGET"
status=$?
set -e

if [ "$status" -eq 124 ]; then
  echo "afl-fuzz smoke completed after ${FUZZ_SECONDS}s"
  exit 0
fi

exit "$status"
