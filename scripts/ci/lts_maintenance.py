#!/usr/bin/env python3
"""Weekly LTS maintenance.

Reads LTS.json from the working tree (master). For each line that is still
inside its two-year window, dispatches CI on that branch. For a line past
EOL, or a release/* branch that LTS.json does not mention, opens an issue
if one is not already open. Prints the plan with --dry-run and does not
call gh.
"""

from __future__ import annotations

import argparse
import json
import subprocess
from datetime import date, datetime
from pathlib import Path


def run(cmd: list[str], check: bool = True) -> subprocess.CompletedProcess[str]:
    return subprocess.run(cmd, check=check, text=True, capture_output=True)


def active_remote_branches() -> list[str]:
    result = run(["git", "ls-remote", "--heads", "origin", "refs/heads/release/*"])
    names: list[str] = []
    for line in result.stdout.splitlines():
        if not line.strip():
            continue
        ref = line.split()[-1]
        names.append(ref.removeprefix("refs/heads/"))
    return names


def open_issues(title: str) -> list[int]:
    result = run(
        [
            "gh",
            "issue",
            "list",
            "--state",
            "open",
            "--search",
            f"{title} in:title",
            "--json",
            "number,title",
        ]
    )
    found = []
    for item in json.loads(result.stdout or "[]"):
        if item.get("title") == title:
            found.append(item["number"])
    return found


def ensure_label(name: str, color: str, description: str) -> None:
    listed = run(["gh", "label", "list", "--search", name, "--json", "name"], check=False)
    names = []
    if listed.returncode == 0 and listed.stdout.strip():
        names = [item["name"] for item in json.loads(listed.stdout)]
    if name in names:
        return
    run(["gh", "label", "create", name, "--color", color, "--description", description], check=False)


def open_issue(title: str, body: str, label: str) -> None:
    if open_issues(title):
        print(f"issue already open: {title}")
        return
    run(["gh", "issue", "create", "--title", title, "--body", body, "--label", label])
    print(f"opened issue: {title}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path("."))
    parser.add_argument("--today", default="")
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()
    today = datetime.strptime(args.today, "%Y-%m-%d").date() if args.today else date.today()

    document = json.loads((args.root / "LTS.json").read_text())
    if document.get("policy", {}).get("support_years") != 2:
        raise SystemExit("LTS.json policy.support_years must be 2")

    recorded: dict[str, dict] = {}
    for line in document.get("lines", []):
        recorded[line["branch"]] = line

    remote = active_remote_branches() if not args.dry_run else []
    if args.dry_run:
        # Still report what the file says. Remote discovery needs the network;
        # --dry-run skips gh and git ls-remote so a local check stays offline.
        remote = []

    plan: list[str] = []
    for branch, line in recorded.items():
        eol = datetime.strptime(line["eol_date"], "%Y-%m-%d").date()
        if today > eol:
            plan.append(f"eol {branch} since {line['eol_date']}")
        else:
            plan.append(f"ci {branch} until {line['eol_date']}")
    for branch in remote:
        if branch not in recorded:
            plan.append(f"untracked {branch}")

    for item in plan:
        print(item)
    if args.dry_run:
        return

    if not remote:
        remote = active_remote_branches()

    ensure_label("lts-eol", "B60205", "Release branch is past its two-year support window")
    ensure_label("lts-untracked", "FBCA04", "release/* branch is missing from LTS.json")

    for branch, line in recorded.items():
        eol = datetime.strptime(line["eol_date"], "%Y-%m-%d").date()
        if today > eol:
            open_issue(
                f"LTS past EOL: {branch}",
                "\n".join(
                    [
                        f"`{branch}` reached its end of support on {line['eol_date']} "
                        f"(cut {line['cut_date']}, two-year window).",
                        "",
                        "The weekly maintenance job no longer dispatches CI for it.",
                        "",
                        f"- Stop adding the `backport {branch}` label.",
                        "- Remove its `github-actions` entry from `.github/dependabot.yml`.",
                        "- Delete the branch when you are ready. The ruleset blocks deletion until an admin bypasses it or the ruleset is edited.",
                        "",
                        "The branch is not deleted automatically.",
                    ]
                ),
                "lts-eol",
            )
            continue
        if branch not in remote:
            print(f"::warning::{branch} is recorded but not on the remote; not dispatching CI")
            continue
        dispatched = run(["gh", "workflow", "run", "ci.yml", "--ref", branch], check=False)
        if dispatched.returncode != 0:
            print(f"::warning::could not dispatch CI for {branch}: {dispatched.stderr.strip()}")
        else:
            print(f"dispatched CI for {branch}")

    for branch in remote:
        if branch in recorded:
            continue
        open_issue(
            f"Untracked release branch: {branch}",
            "\n".join(
                [
                    f"`{branch}` exists on the remote and is not listed in `LTS.json`.",
                    "",
                    "Cut lines are recorded by the pull request that `cut-lts.yml` opens. "
                    "If that pull request is still open, merge it. Otherwise add the line "
                    "(version, cut commit, cut date, EOL two years later) or delete the branch.",
                ]
            ),
            "lts-untracked",
        )


if __name__ == "__main__":
    main()
