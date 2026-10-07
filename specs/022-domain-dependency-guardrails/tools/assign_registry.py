#!/usr/bin/env python3
"""Assign every bootstrap edge to exactly one registry entry (spec 022 T041).

Reads one archcheck JSON report per platform (`archcheck --format json`, taken
with an empty allowlist), applies the ordered first-match rules below, and
writes tests/architecture/allowlist.txt. An edge present on every given
platform is untagged; an edge seen on one platform only gets that platform's
tag. Exits nonzero if any edge matches no rule.

    tools/assign_registry.py --macos build/arch-bootstrap.json \
        [--linux linux-bootstrap.json] [--out tests/architecture/allowlist.txt]

The rule table is mirrored in registry-reconciliation.md; keep them in step.
"""

import argparse
import collections
import json
import sys

# Edges the audit flagged as vestigial includes (V19) that are violations
# today. Phase 6 deletes the includes and these lines with them.
V19 = {
    ("src/ri/state/options.cpp", "src/ri/texture/texture.h"),
    ("src/ri/state/attributes.cpp", "src/ri/hiders/irradiance.h"),
    ("src/libshader/shading/shading.cpp", "src/ri/hiders/raytracer.h"),
    ("src/libshader/shading/shading.cpp", "src/ri/render/remoteChannel.h"),
    ("src/libshader/shading/shading.cpp", "src/ri/geometry/points.h"),
    ("src/ri/texture/pointCloud.cpp", "src/ri/geometry/object.h"),
    ("src/ri/texture/pointCloud.cpp", "src/ri/hiders/raytracer.h"),
}

# Infrastructure that lives in the wrong place (V13): diagnostics, the RI and
# renderer-core vocabulary, variable parsing, the global memory arena.
V13_HEADERS = {
    "src/ri/parse/error.h",
    "src/ri/parse/ri.h",
    "src/ri/parse/variable.h",
    "src/ri/render/rendererc.h",
}
V13_SYMBOLS = {"error", "warning", "info", "fatal", "parseVariable", "CRenderer::globalMemory"}


def symbol_name(edge):
    """Unqualified display name of a link target ("CRenderer::locateFile")."""
    return (edge.get("display") or edge["to"]).split("(")[0]


def is_dispatch(edge):
    return edge["kind"] == "link" and any(
        "Dispatch" in f or "Stub" in f for f in edge.get("defined_in", []))


def is_v13(edge):
    if edge["from_class"] == "Infra":
        return True
    if edge["kind"] == "include":
        return edge["to"] in V13_HEADERS
    name = symbol_name(edge)
    return name in V13_SYMBOLS or name.startswith("RI_")


def is_compiler(edge):
    return edge["from"].startswith(("src/libshader/compiler/", "@build/src/libshader/compiler/"))


# (registry id, description, predicate) -- first match wins.
RULES = [
    ("DB-022", "V19 vestigial include",
     lambda e, f, t: e["kind"] == "include" and (e["from"], e["to"]) in V19),
    ("DB-001", "Shading link reference to the renderer core / Hiders / Scene Interface",
     lambda e, f, t: f == "Shading" and e["kind"] == "link"),
    ("DB-025", "domain reaches the data viewer (Consumer)",
     lambda e, f, t: "Consumer" in t),
    ("DB-026", "Texturing uses the RGBE codec in Display Outputs",
     lambda e, f, t: f == "Texturing" and "Display" in t),
    ("DB-017", "symbol defined by a *Dispatch/*DispatchStub link-time pair",
     lambda e, f, t: is_dispatch(e)),
    ("DB-019", "shader compiler / DSO prototype parser coupling",
     lambda e, f, t: is_compiler(e) or e["to"] == "src/ri/parse/dso.h"),
    ("DB-016", "misplaced infrastructure (diagnostics, vocabulary, variables, arena)",
     lambda e, f, t: is_v13(e)),
    ("DB-004", "Geometry -> renderer core / Scene Interface",
     lambda e, f, t: f == "Geometry" and t & {"Core", "SceneInterface"}),
    ("DB-006", "Geometry <-> Shading / Hiders coupling",
     lambda e, f, t: (f == "Geometry" and t & {"Shading", "Hiders"}) or (f == "Shading" and "Geometry" in t)),
    ("DB-005", "Shading includes hider headers (shared CShadingContext)",
     lambda e, f, t: f == "Shading" and "Hiders" in t),
    ("DB-001", "Shading includes renderer-core headers",
     lambda e, f, t: f == "Shading" and "Core" in t),
    ("DB-008", "Graphics State reaches other domains",
     lambda e, f, t: f == "GraphicsState"),
    ("DB-021", "Texturing reads the AOV/channel registry",
     lambda e, f, t: f == "Texturing" and e["to"] == "src/ri/state/displayChannel.h"),
    ("DB-010", "Texturing depends on Shading / core / state; hiders' GI caches use Texturing",
     lambda e, f, t: f == "Texturing" or (f == "Hiders" and "Texturing" in t)),
    ("DB-011", "Hiders read renderer-core state",
     lambda e, f, t: f == "Hiders" and "Core" in t),
    ("DB-015", "Scene Interface depends on the renderer core",
     lambda e, f, t: f == "SceneInterface" and "Core" in t),
]


# Section titles in the generated allowlist (one per registry id).
TITLES = {
    "DB-001": "Shading depends on the renderer core and the hiders",
    "DB-004": "Geometry depends on the renderer core and on Scene Interface",
    "DB-005": "Hiders and Shading share one class (CShadingContext)",
    "DB-006": "Geometry is coupled to Shading and to the hiders",
    "DB-008": "Graphics State owns other domains' objects",
    "DB-010": "Texturing depends on Shading, the renderer core and Graphics State",
    "DB-011": "Hiders read renderer-core state (CRenderer)",
    "DB-015": "Scene Interface depends on the renderer core",
    "DB-016": "Infrastructure lives in Scene Interface and the renderer core",
    "DB-017": "Link-time *Dispatch / *DispatchStub substitution pairs",
    "DB-019": "Shader compiler depends on Scene Interface / renderer-core headers",
    "DB-021": "Texturing reads the AOV/channel registry",
    "DB-022": "Vestigial cross-domain includes",
    "DB-025": "Domains call into the data viewer (a consumer)",
    "DB-026": "Texturing uses the RGBE codec in Display Outputs",
}


def assign(edge):
    f = edge["from_class"]
    t = set(edge["to_class"])
    for rid, _, pred in RULES:
        if pred(edge, f, t):
            return rid
    return None


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--macos")
    ap.add_argument("--linux")
    ap.add_argument("--out")
    args = ap.parse_args()
    reports = {p: json.load(open(path)) for p, path in (("macos", args.macos), ("linux", args.linux)) if path}
    if not reports:
        ap.error("give at least one of --macos / --linux")

    # (kind, from, to) -> (edge, set of platforms it occurs on)
    seen = {}
    for plat, report in reports.items():
        for e in report["violations"]:
            key = (e["kind"], e["from"], e["to"])
            seen.setdefault(key, (e, set()))[1].add(plat)

    lines = collections.defaultdict(list)
    unmatched = []
    for key in sorted(seen):
        e, plats = seen[key]
        rid = assign(e)
        if rid is None:
            unmatched.append(e)
            continue
        tag = "" if len(plats) == len(reports) else sorted(plats)[0]
        lines[rid].append((e, tag))

    for e in unmatched:
        print(f"unmatched: {e['kind']} {e['from']} -> {e['to']} [{e['from_class']} -> {e['to_class']}]",
              file=sys.stderr)
    if unmatched:
        return 1

    out = ["# archcheck allowlist: one line per known violating edge. Shrink-only.",
           "# Format: specs/022-domain-dependency-guardrails/contracts/data-files.md",
           "#",
           "#   <id>    <kind>   <from>   ->  <to>   [macos|linux]",
           "#",
           "# <id> must be an Open entry in DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md.",
           "# <kind> is include (to = resolved header) or link (to = mangled symbol,",
           "# without the macOS leading underscore). Optional platform tag: macos | linux.",
           "# A new line needs an Open registry entry and explicit reviewer sign-off.",
           "#",
           "# Seeded by specs/022-domain-dependency-guardrails/tools/assign_registry.py."]
    for rid in sorted(lines):
        out.append("")
        out.append(f"# --- {rid}: {TITLES[rid]} ({len(lines[rid])}) ---")
        for e, tag in lines[rid]:
            row = f"{rid}  {e['kind']:<7}  {e['from']}  ->  {e['to']}"
            if tag:
                row += f"  {tag}"
            if e["kind"] == "link":
                row += f"   # {e['display']}"
            out.append(row)
    text = "\n".join(out) + "\n"
    if args.out:
        open(args.out, "w").write(text)
    else:
        sys.stdout.write(text)
    for rid in sorted(lines):
        kinds = collections.Counter(e["kind"] for e, _ in lines[rid])
        print(f"{rid}  {len(lines[rid]):4d}  include={kinds['include']:<4d} link={kinds['link']:<4d} {TITLES[rid]}",
              file=sys.stderr)
    print(f"total {sum(len(v) for v in lines.values())}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
