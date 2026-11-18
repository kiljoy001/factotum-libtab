#!/usr/bin/env bash
# Runs the local quality gate.
#
#   tools/ci.sh          fast gate: build, native checks, C# tests, coverage, CRAP
#   tools/ci.sh --full   also runs sanitizer, native fuzz/mutation, Stryker, and AFL

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

FULL=0
if [[ "${1:-}" == "--full" ]]; then
  FULL=1
fi

MIN_LINE="${MIN_LINE:-85}"
MIN_BRANCH="${MIN_BRANCH:-75}"
CRAP_LIMIT="${CRAP_LIMIT:-30}"
MIN_MUTATION="${MIN_MUTATION:-90}"

step() { printf '\n\033[1m== %s\033[0m\n' "$1"; }

step "native build"
make factotum factotum-smoke-test

step "native unit and property checks"
make check

step "native 9P integration"
make check-integration

step "native CRAP score"
make check-crap

step "C# static analysis build"
dotnet build tests/Factotum.Testing/Factotum.Testing.csproj -c Release
dotnet build tests/Factotum.Tests/Factotum.Tests.csproj -c Release
dotnet build tests/Factotum.Specs/Factotum.Specs.csproj -c Release

mkdir -p artifacts/coverage

step "C# unit, property, and fuzz-harness tests"
dotnet test tests/Factotum.Tests/Factotum.Tests.csproj \
  -c Release --no-build \
  /p:CollectCoverage=true \
  /p:CoverletOutputFormat=json \
  /p:CoverletOutput="$ROOT/artifacts/coverage/merged.json" \
  /p:Include="[Factotum.Testing]*"

step "C# behaviour specs"
dotnet test tests/Factotum.Specs/Factotum.Specs.csproj \
  -c Release --no-build \
  /p:CollectCoverage=true \
  /p:CoverletOutputFormat=cobertura \
  /p:CoverletOutput="$ROOT/artifacts/coverage/dotnet.xml" \
  /p:MergeWith="$ROOT/artifacts/coverage/merged.json" \
  /p:Include="[Factotum.Testing]*"

step "C# gate self-tests"
python3 tools/test_crap.py
python3 tools/test_coverage_report.py

step "C# coverage thresholds"
python3 tools/coverage_gate.py --min-line "$MIN_LINE" --min-branch "$MIN_BRANCH"

step "C# CRAP score"
python3 tools/crap.py \
  --threshold "$CRAP_LIMIT" --fail-over "$CRAP_LIMIT" \
  --json artifacts/crap-dotnet.json

if [[ $FULL -eq 1 ]]; then
  step "native sanitizer checks"
  make check-asan

  step "native fuzz smoke"
  make check-fuzz

  step "native mutation smoke"
  make check-mutant

  step "C# mutation testing"
  MIN_MUTATION="$MIN_MUTATION" bash tools/stryker_gate.sh

  step "C# AFL fuzz campaign"
  tests/fuzz.sh attr
fi

printf '\n\033[32mpipeline passed\033[0m\n'
