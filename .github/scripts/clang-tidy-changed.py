#!/usr/bin/env python3
"""Runs clang-tidy on the lines a change touches, and reports what it finds to GitHub.

Usage: clang-tidy-changed.py <base commit> <build folder with compile_commands.json>

Only the lines added or changed since <base> are checked (clang-tidy's -line-filter), so the
warnings the inherited code already has stay out of the way. A changed .cpp file is analysed
directly; a changed header is analysed through a .cpp file of the build that includes it.

Each warning becomes a GitHub annotation on its line, and a table goes to the job summary.
The exit status is 1 when there are warnings and the STRICT environment variable is "true",
0 otherwise.
"""

import json
import os
import re
import shutil
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

SOURCE_DIRS = ("es-app/", "es-core/", "es-launcher/")
SOURCE_EXT = (".cpp", ".cc", ".cxx")
HEADER_EXT = (".h", ".hpp")
DIAG_RE = re.compile(r"^(?P<file>/[^:]+):(?P<line>\d+):(?P<col>\d+): (?P<level>warning|error): (?P<msg>.*?)(?: \[(?P<check>[^\]]+)\])?$")


def changed_lines(base):
    """{path relative to the repo: [[first, last], ...]} of the lines added since base."""
    diff = subprocess.run(
        ["git", "diff", "-U0", "--no-color", "--diff-filter=AM", base, "HEAD", "--", *SOURCE_DIRS],
        check=True, capture_output=True, text=True).stdout
    lines, current = {}, None
    for row in diff.splitlines():
        if row.startswith("+++ "):
            name = row[4:]
            current = name[2:] if name.startswith("b/") else None
            if current and not current.endswith(SOURCE_EXT + HEADER_EXT):
                current = None
        elif row.startswith("@@") and current:
            m = re.match(r"@@ -\S+ \+(\d+)(?:,(\d+))? @@", row)
            start, count = int(m.group(1)), int(m.group(2) or 1)
            if count:
                lines.setdefault(current, []).append([start, start + count - 1])
    return lines


def files_to_analyse(changed, compiled, root):
    """The .cpp files to run clang-tidy on: the changed ones, plus one includer per changed header."""
    files = {f for f in changed if f.endswith(SOURCE_EXT) and f in compiled}
    for header in (f for f in changed if f.endswith(HEADER_EXT)):
        stem = Path(header).with_suffix("")
        # the header's own .cpp first (Foo.h -> Foo.cpp), else the first compiled file including it
        same = [c for c in compiled if Path(c).with_suffix("") == stem]
        if not same:
            same = [c for c in compiled if c.startswith(SOURCE_DIRS) and Path(header).name in
                    (root / c).read_text(errors="replace")]
        if same:
            files.add(sorted(same)[0])
        else:
            print(f"::notice file={header}::No compiled file includes this header, so clang-tidy did not check it.")
    return sorted(files)


def main():
    base, build = sys.argv[1], Path(sys.argv[2])
    root = Path.cwd()
    clang_tidy = os.environ.get("CLANG_TIDY") or shutil.which("clang-tidy")

    changed = changed_lines(base)
    if not changed:
        print("No C++ line changed in es-app, es-core or es-launcher.")
        summary("No C++ line changed, nothing to check.")
        return 0

    commands = json.loads((build / "compile_commands.json").read_text())
    compiled = {os.path.relpath(c["file"], root) for c in commands}
    files = files_to_analyse(changed, compiled, root)
    line_filter = json.dumps([{"name": str(root / f), "lines": r} for f, r in changed.items()])

    def run(f):
        r = subprocess.run([clang_tidy, "-p", str(build), "--quiet", f"--line-filter={line_filter}",
                            "--extra-arg=-Wno-unknown-warning-option", f],
                           capture_output=True, text=True)
        return f, r.stdout

    print(f"Running clang-tidy on {len(files)} file(s) for {len(changed)} changed file(s).")
    found, seen, broken = [], set(), []
    with ThreadPoolExecutor(os.cpu_count() or 2) as pool:
        for f, out in pool.map(run, files):
            for row in out.splitlines():
                m = DIAG_RE.match(row)
                if not m:
                    continue
                if m["level"] == "error" and m["check"] in (None, "clang-diagnostic-error"):
                    broken.append(f)  # clang could not parse it: not the author's fault
                    continue
                path = os.path.relpath(m["file"], root)
                key = (path, m["line"], m["col"], m["msg"])
                if key in seen or path not in changed:
                    continue
                seen.add(key)
                found.append({"file": path, "line": int(m["line"]), "col": int(m["col"]),
                              "msg": m["msg"], "check": m["check"] or "clang"})

    found.sort(key=lambda w: (w["file"], w["line"], w["col"]))
    for w in found:
        print(f"::warning file={w['file']},line={w['line']},col={w['col']},"
              f"title={w['check']}::{w['msg']}")
    for f in sorted(set(broken)):
        print(f"::notice file={f}::clang could not parse this file, so some of its lines may not have been checked.")

    if found:
        rows = "\n".join("| `{file}:{line}` | `{check}` | {text} |".format(**w, text=w["msg"].replace("|", "\\|"))
                         for w in found)
        summary(f"clang-tidy found **{len(found)}** warning(s) on the changed lines.\n\n"
                f"| Where | Check | Message |\n|---|---|---|\n{rows}\n\n"
                "Fix them, or excuse a line with `// NOLINT(check-name)` and a reason.")
    else:
        summary(f"clang-tidy found nothing on the changed lines ({len(files)} file(s) analysed).")
    print(f"{len(found)} warning(s).")
    return 1 if found and os.environ.get("STRICT") == "true" else 0


def summary(text):
    path = os.environ.get("GITHUB_STEP_SUMMARY")
    if path:
        with open(path, "a") as out:
            out.write("### clang-tidy\n\n" + text + "\n")


if __name__ == "__main__":
    sys.exit(main())
