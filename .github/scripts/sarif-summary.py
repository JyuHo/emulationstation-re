#!/usr/bin/env python3
"""Prints the results of SARIF files (CodeQL's output) as a table, in the log and in the job summary.

Usage: sarif-summary.py <file.sarif>...
"""

import json
import os
import sys

LEVELS = {"error": 0, "warning": 1, "note": 2, "none": 3}


def main():
    rows = []
    for path in sys.argv[1:]:
        with open(path) as f:
            sarif = json.load(f)
        for run in sarif.get("runs", []):
            rules = {}
            for component in [run["tool"]["driver"], *run["tool"].get("extensions", [])]:
                for rule in component.get("rules", []):
                    rules[rule["id"]] = rule
            for result in run.get("results", []):
                rule = rules.get(result.get("ruleId"), {})
                props = rule.get("properties", {})
                level = result.get("level") or rule.get("defaultConfiguration", {}).get("level", "warning")
                where = result.get("locations", [{}])[0].get("physicalLocation", {})
                rows.append({
                    "severity": props.get("security-severity", ""),
                    "level": level,
                    "rule": result.get("ruleId", "?"),
                    "file": where.get("artifactLocation", {}).get("uri", "?"),
                    "line": where.get("region", {}).get("startLine", 0),
                    "message": result.get("message", {}).get("text", "").replace("\n", " "),
                })

    rows.sort(key=lambda r: (-float(r["severity"] or 0), LEVELS.get(r["level"], 9), r["rule"], r["file"], r["line"]))
    lines = [f"{len(rows)} alert(s).", "",
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
