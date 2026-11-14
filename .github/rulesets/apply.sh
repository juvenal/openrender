#!/usr/bin/env bash
# Create or update the two branch rulesets in protect-master-and-release.json.
# Requires an admin token: gh auth status, then run from the repository root.
# This does not run in CI. See docs/RELEASING.md.
set -euo pipefail

repo="${1:-}"
if [ -z "${repo}" ]; then
    repo="$(gh repo view --json nameWithOwner --jq .nameWithOwner)"
fi

python3 - "${repo}" <<'PY'
import json
import subprocess
import sys

repo = sys.argv[1]
document = json.load(open(".github/rulesets/protect-master-and-release.json"))
existing = json.loads(subprocess.check_output(["gh", "api", f"repos/{repo}/rulesets"], text=True))
by_name = {item["name"]: item["id"] for item in existing}

for ruleset in document["rulesets"]:
    payload = json.dumps(ruleset)
    name = ruleset["name"]
    if name in by_name:
        print(f"updating {name} ({by_name[name]})")
        subprocess.run(
            ["gh", "api", "--method", "PUT", f"repos/{repo}/rulesets/{by_name[name]}", "--input", "-"],
            input=payload,
            text=True,
            check=True,
        )
    else:
        print(f"creating {name}")
        subprocess.run(
            ["gh", "api", "--method", "POST", f"repos/{repo}/rulesets", "--input", "-"],
            input=payload,
            text=True,
            check=True,
        )
PY
