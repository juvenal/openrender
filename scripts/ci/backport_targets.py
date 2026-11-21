#!/usr/bin/env python3
"""Decide which release branches a merged pull request should be backported to.

Writes GitHub Actions outputs:
  mode      "labels" (honor backport <branch> labels), "all" (every active line), or "none"
  branches  space-separated branch names when mode is "all"
"""

from __future__ import annotations

import json
import os
import re
from datetime import date, datetime
from pathlib import Path


def main() -> None:
    event_path = os.environ["GITHUB_EVENT_PATH"]
    output_path = os.environ["GITHUB_OUTPUT"]
    event = json.loads(Path(event_path).read_text())
    labels = [item["name"] for item in event["pull_request"]["labels"]]
    specific = [name.split(" ", 1)[1] for name in labels if name.startswith("backport ") and name != "backport "]
    bare = "backport" in labels

    mode = "none"
    branches: list[str] = []
    if specific:
        # korthout/backport-action reads these labels itself.
        mode = "labels"
    elif bare:
        document = json.loads(Path("LTS.json").read_text())
        today = date.today()
        for line in document.get("lines", []):
            eol = datetime.strptime(line["eol_date"], "%Y-%m-%d").date()
            if line.get("status", "active") == "active" and today <= eol:
                if re.fullmatch(r"release/[0-9]+\.[0-9]+", line["branch"]):
                    branches.append(line["branch"])
        mode = "all" if branches else "none"

    with Path(output_path).open("a") as handle:
        handle.write(f"mode={mode}\n")
        handle.write(f"branches={' '.join(branches)}\n")
    print(f"backport mode={mode} branches={' '.join(branches)}")


if __name__ == "__main__":
    main()
