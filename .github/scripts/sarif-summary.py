#!/usr/bin/env python3
"""Prints the results of SARIF files (CodeQL's output) as a table, in the log and in the job summary.

Usage: sarif-summary.py [--exclude <path prefix>]... <file.sarif>...

--exclude removes the results under that path from the files themselves, before they are
uploaded (CodeQL's paths-ignore has no effect on compiled languages).
"""

import json
import os
import sys

LEVELS = {"error": 0, "warning": 1, "note": 2, "none": 3}


def location(result):
    return result.get("locations", [{}])[0].get("physicalLocation", {})


def main():
    args, excluded = sys.argv[1:], []
    while args and args[0] == "--exclude":
        excluded.append(args[1])
        args = args[2:]

    rows, dropped = [], 0
    for path in args:
        with open(path) as f:
            sarif = json.load(f)
        if excluded:
            for run in sarif.get("runs", []):
                kept = [r for r in run.get("results", [])
                        if not location(r).get("artifactLocation", {}).get("uri", "").startswith(tuple(excluded))]
                dropped += len(run.get("results", [])) - len(kept)
                run["results"] = kept
            with open(path, "w") as f:
                json.dump(sarif, f)
        for run in sarif.get("runs", []):
            rules = {}
            for component in [run["tool"]["driver"], *run["tool"].get("extensions", [])]:
                for rule in component.get("rules", []):
                    rules[rule["id"]] = rule
            for result in run.get("results", []):
                rule = rules.get(result.get("ruleId"), {})
                props = rule.get("properties", {})
                level = result.get("level") or rule.get("defaultConfiguration", {}).get("level", "warning")
                where = location(result)
                rows.append({
                    "severity": props.get("security-severity", ""),
                    "level": level,
                    "rule": result.get("ruleId", "?"),
                    "file": where.get("artifactLocation", {}).get("uri", "?"),
                    "line": where.get("region", {}).get("startLine", 0),
                    "message": result.get("message", {}).get("text", "").replace("\n", " "),
                })

    rows.sort(key=lambda r: (-float(r["severity"] or 0), LEVELS.get(r["level"], 9), r["rule"], r["file"], r["line"]))
    lines = [f"{len(rows)} alert(s)." + (f" {dropped} left out, under {', '.join(excluded)}." if dropped else ""), "",
             "| Severity | Level | Rule | Where | Message |", "|---|---|---|---|---|"]
    for r in rows:
        message = r["message"].replace("|", "\\|")[:300]
        lines.append(f"| {r['severity']} | {r['level']} | `{r['rule']}` | `{r['file']}:{r['line']}` | {message} |")
    text = "\n".join(lines)
    print(text)
    summary = os.environ.get("GITHUB_STEP_SUMMARY")
    if summary:
        with open(summary, "a") as out:
            out.write("### CodeQL alerts\n\n" + text + "\n")


if __name__ == "__main__":
    main()
