#!/usr/bin/env python3
"""Record a newly cut release/X.Y line in LTS.json and dependabot.yml.

The cut workflow runs this on a branch and opens a pull request. It does not
push to master.
"""

from __future__ import annotations

import argparse
import json
import re
from datetime import date, datetime
from pathlib import Path


MARKER = "# LTS-DEPENDABOT-ENTRIES"


def add_years(day: date, years: int) -> date:
    try:
        return day.replace(year=day.year + years)
    except ValueError:
        # 29 February has no counterpart two years later.
        return date(day.year + years, 2, 28)


def dependabot_block(version: str) -> str:
    group = "github-actions-release-" + version.replace(".", "-")
    branch = f"release/{version}"
    return (
        "  - package-ecosystem: github-actions\n"
        '    directory: "/"\n'
        "    schedule:\n"
        "      interval: weekly\n"
        f"    target-branch: {branch}\n"
        "    labels:\n"
        "      - dependencies\n"
        "    commit-message:\n"
        "      prefix: ci\n"
        "    groups:\n"
        f"      {group}:\n"
        '        patterns:\n'
        '          - "*"\n'
    )


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path("."))
    parser.add_argument("--version", required=True)
    parser.add_argument("--commit", required=True)
    parser.add_argument("--cut-date", required=True)
    args = parser.parse_args()

    if not re.fullmatch(r"[0-9]+\.[0-9]+", args.version):
        raise SystemExit(f"version must be X.Y, got {args.version!r}")
    if not re.fullmatch(r"[0-9a-f]{40}", args.commit):
        raise SystemExit("commit must be a 40-character sha")
    cut = datetime.strptime(args.cut_date, "%Y-%m-%d").date()
    eol = add_years(cut, 2)

    root = args.root
    lts_path = root / "LTS.json"
    document = json.loads(lts_path.read_text())
    if document.get("policy", {}).get("support_years") != 2:
        raise SystemExit("LTS.json policy.support_years must be 2")
    lines = document.setdefault("lines", [])
    if any(item.get("version") == args.version for item in lines):
        raise SystemExit(f"release/{args.version} is already recorded")
    lines.append(
        {
            "version": args.version,
            "branch": f"release/{args.version}",
            "cut_commit": args.commit,
            "cut_date": cut.isoformat(),
            "eol_date": eol.isoformat(),
            "first_tag": f"v{args.version}.0",
            "status": "active",
        }
    )
    lts_path.write_text(json.dumps(document, indent=2) + "\n")

    dependabot = root / ".github" / "dependabot.yml"
    text = dependabot.read_text()
    branch = f"release/{args.version}"
    if f"target-branch: {branch}\n" in text:
        return
    if MARKER not in text:
        raise SystemExit(f"{dependabot} is missing the {MARKER} marker")
    block = dependabot_block(args.version)
    dependabot.write_text(text.replace(MARKER, block + MARKER, 1))


if __name__ == "__main__":
    main()
