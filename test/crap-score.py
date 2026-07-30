#!/usr/bin/env python3
import argparse
import gzip
import json
import pathlib
import re
import sys


CONTROL_WORDS = ("if", "for", "while", "case")


def strip_comments_and_strings(text):
    out = []
    i = 0
    state = "code"
    while i < len(text):
        c = text[i]
        n = text[i + 1] if i + 1 < len(text) else ""
        if state == "code":
            if c == "/" and n == "/":
                out.extend("  ")
                i += 2
                state = "line"
                continue
            if c == "/" and n == "*":
                out.extend("  ")
                i += 2
                state = "block"
                continue
            if c == '"':
                out.append(" ")
                i += 1
                state = "string"
                continue
            if c == "'":
                out.append(" ")
                i += 1
                state = "char"
                continue
            out.append(c)
            i += 1
            continue
        if state == "line":
            out.append("\n" if c == "\n" else " ")
            i += 1
            if c == "\n":
                state = "code"
            continue
        if state == "block":
            out.append("\n" if c == "\n" else " ")
            if c == "*" and n == "/":
                out.append(" ")
                i += 2
                state = "code"
            else:
                i += 1
            continue
        if state == "string":
            out.append("\n" if c == "\n" else " ")
            if c == "\\" and n:
                out.append(" ")
                i += 2
            elif c == '"':
                i += 1
                state = "code"
            else:
                i += 1
            continue
        if state == "char":
            out.append("\n" if c == "\n" else " ")
            if c == "\\" and n:
                out.append(" ")
                i += 2
            elif c == "'":
                i += 1
                state = "code"
            else:
                i += 1
            continue
    return "".join(out)


def complexity(source, start, end):
    body = "\n".join(source.splitlines()[start - 1:end])
    body = strip_comments_and_strings(body)
    score = 1
    for word in CONTROL_WORDS:
        score += len(re.findall(r"\b" + word + r"\b", body))
    score += body.count("&&")
    score += body.count("||")
    score += body.count("?")
    return score


def rel(path, root):
    p = pathlib.Path(path)
    if p.is_absolute():
        try:
            return p.relative_to(root).as_posix()
        except ValueError:
            return p.as_posix()
    return p.as_posix()


def load_gcov(json_dir):
    files = {}
    for path in pathlib.Path(json_dir).glob("*.gcov.json.gz"):
        with gzip.open(path, "rt", encoding="utf-8") as f:
            data = json.load(f)
        for entry in data.get("files", []):
            files[entry["file"]] = entry
    return files


def function_coverage(entry, fn):
    lines = [
        line for line in entry.get("lines", [])
        if line.get("function_name") == fn["name"]
        and fn["start_line"] <= line.get("line_number", 0) <= fn["end_line"]
    ]
    if not lines:
        return 1.0 if fn.get("execution_count", 0) > 0 else 0.0
    hit = sum(1 for line in lines if line.get("count", 0) > 0)
    return hit / len(lines)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", required=True)
    parser.add_argument("--json-dir", required=True)
    parser.add_argument("--threshold", type=float, default=30.0)
    parser.add_argument("--top", type=int, default=20)
    parser.add_argument("targets", nargs="*")
    args = parser.parse_args()

    root = pathlib.Path(args.root).resolve()
    targets = {rel(target, root) for target in args.targets}
    gcov_files = load_gcov(args.json_dir)
    rows = []

    for filename, entry in gcov_files.items():
        filename = rel(filename, root)
        if targets and filename not in targets:
            continue
        source_path = root / filename
        if not source_path.exists():
            print(f"missing source for coverage: {filename}", file=sys.stderr)
            return 2
        source = source_path.read_text(encoding="utf-8", errors="replace")
        for fn in entry.get("functions", []):
            comp = complexity(source, fn["start_line"], fn["end_line"])
            cov = function_coverage(entry, fn)
            crap = comp * comp * ((1.0 - cov) ** 3) + comp
            rows.append((crap, comp, cov, filename, fn))

    if not rows:
        print("CRAP failed: no scored functions", file=sys.stderr)
        return 2

    rows.sort(reverse=True, key=lambda row: row[0])
    print(f"CRAP scores threshold={args.threshold:.1f}")
    for crap, comp, cov, filename, fn in rows[:args.top]:
        print(
            f"{crap:6.1f} cc={comp:2d} cov={cov*100:5.1f}% "
            f"{fn['name']} {filename}:{fn['start_line']}"
        )
    if len(rows) > args.top:
        print(f"... {len(rows) - args.top} more scored functions")

    failures = [row for row in rows if row[0] > args.threshold]
    if failures:
        print(f"CRAP failed: {len(failures)} functions over {args.threshold:.1f}")
        return 1
    print(f"CRAP ok: {len(rows)} functions scored")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
