#!/usr/bin/env python3
"""Find and validate the merged C# Cobertura coverage report."""

from __future__ import annotations

import sys
import time
from pathlib import Path

DEFAULT_REPORT = Path("artifacts/coverage/dotnet.xml")
MAX_AGE_SECONDS = 60 * 60


def find(root: Path | None = None) -> Path:
    here = (root or Path.cwd()).resolve()

    for candidate in [here, *here.parents]:
        if (candidate / "tools" / "crap.py").exists():
            return candidate / DEFAULT_REPORT

    raise FileNotFoundError(f"no repository root above {here}: looked for tools/crap.py")


def check(report: Path, max_age: int = MAX_AGE_SECONDS) -> list[str]:
    problems: list[str] = []

    if not report.exists():
        problems.append(
            f"no coverage report at {report}; run tools/ci.sh or make check-dotnet-coverage"
        )
        return problems

    age = time.time() - report.stat().st_mtime
    if age > max_age:
        problems.append(
            f"the coverage report is {age / 60:.0f} minutes old, older than the "
            f"{max_age // 60} minute limit"
        )

    newer = _sources_newer_than(report)
    if newer:
        root = report.parent.parent.parent
        shown = ", ".join(str(path.relative_to(root)) for path in newer[:3])
        more = f" and {len(newer) - 3} more" if len(newer) > 3 else ""
        problems.append(f"source changed after the report was written: {shown}{more}")

    return problems


def resolve(argument: Path | None, max_age: int = MAX_AGE_SECONDS) -> Path:
    report = argument if argument is not None else find()
    problems = check(report, max_age)

    if problems:
        for problem in problems:
            print(f"FAIL: {problem}", file=sys.stderr)
        sys.exit(2)

    return report


def _sources_newer_than(report: Path) -> list[Path]:
    root = report.parent.parent.parent
    written = report.stat().st_mtime
    tests = root / "tests"

    if not tests.is_dir():
        return []

    return sorted(
        path
        for path in tests.rglob("*.cs")
        if "obj" not in path.parts
        and "bin" not in path.parts
        and path.stat().st_mtime > written
    )
