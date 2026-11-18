#!/usr/bin/env bash
# Runs Stryker and cleans private factotum-spec services left by timeout mutants.

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

cleanup_factotum_specs() {
  local pids=()
  local pid

  while IFS= read -r pid; do
    [[ -n "$pid" ]] && pids+=("$pid")
  done < <(pgrep -f "$ROOT/factotum -n -s factotum-spec-" || true)

  while IFS= read -r pid; do
    [[ -n "$pid" ]] && pids+=("$pid")
  done < <(pgrep -f "9pserve -u unix!/tmp/factotum-spec-" || true)

  if [[ ${#pids[@]} -gt 0 ]]; then
    kill "${pids[@]}" 2>/dev/null || true
  fi
}

trap cleanup_factotum_specs EXIT
cleanup_factotum_specs

dotnet stryker --reporter json --reporter progress
python3 tools/mutation_summary.py --min-score "${MIN_MUTATION:-90}"
