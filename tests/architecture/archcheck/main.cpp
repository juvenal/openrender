/**
 * Project: openRender
 *
 * File: main.cpp
 *
 * Description:
 *   archcheck: checks the build tree against constitution Principle VIII's
 *   allowed-dependency table (spec 022). Command line per
 *   specs/022-domain-dependency-guardrails/contracts/archcheck-cli.md.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>

#include "config.h"
#include "graph.h"
#include "includes.h"
#include "metrics.h"
#include "report.h"
#include "symbols.h"

using namespace archcheck;

namespace {

    namespace fs = std::filesystem;

    struct Options {
            std::string sourceDir;
            std::string buildDir;
            std::string configDir;
            std::string registry;
            std::string platform;
            std::string format = "text";
            bool emitAllowlist = false;
            bool quiet = false;
    };

    void printUsage(FILE *out) {
        std::fputs("usage: archcheck --source-dir <repo> --build-dir <build> --config <dir>\n"
                   "                 [--platform macos|linux] [--format text|json]\n"
                   "                 [--emit-allowlist] [--registry <file>] [--quiet]\n"
                   "       archcheck --help\n"
                   "\n"
                   "Checks a CMake build tree against the Principle VIII allowed-dependency\n"
                   "table (include edges from compiler depfiles, link edges from nm).\n"
                   "\n"
                   "  --source-dir      repository root; only <source-dir>/src/ is scanned\n"
                   "  --build-dir       CMake build tree holding objects and depfiles\n"
                   "  --config          directory with domain-map.txt, domain-rules.txt,\n"
                   "                    allowlist.txt and metrics-baseline.txt\n"
                   "  --registry        registry markdown file\n"
                   "                    (default <source-dir>/DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md)\n"
                   "  --platform        allowlist platform tag to apply (default: host)\n"
                   "  --format          text (default) or json\n"
                   "  --emit-allowlist  print every current non-allowed edge as allowlist lines\n"
                   "  --quiet           text mode: only failures, warnings and the summary\n"
                   "\n"
                   "Data files (in --config):\n"
                   "  domain-map.txt        file/prefix -> class (SceneInterface, GraphicsState,\n"
                   "                        Geometry, Hiders, Shading, Texturing, ShaderDSO,\n"
                   "                        Procedures, Display, Core, Infra, Consumer)\n"
                   "  domain-rules.txt      the constitution's allowed-dependency table\n"
                   "  allowlist.txt         known violating edges, one per line, each with an\n"
                   "                        Open registry id and an optional macos|linux tag\n"
                   "  metrics-baseline.txt  renderer-core usage baseline per class\n"
                   "\n"
                   "Exit: 0 pass, 1 check failed (violation, stale or invalid allowlist entry,\n"
                   "      failing cycle), 2 configuration or environment error.\n",
                   out);
    }

    // Returns kExitPass to continue, or the exit code to stop with.
    int parseArgs(int argc, char **argv, Options &opt, bool &helpOnly) {
        for (int i = 1; i < argc; i++) {
            const std::string_view arg = argv[i];
            auto value = [&](std::string &dst) {
                if (i + 1 >= argc) {
                    std::fprintf(stderr, "archcheck: %s expects a value\n", argv[i]);
                    return false;
                }
                dst = argv[++i];
                return true;
            };

            if (arg == "--help" || arg == "-h") {
                helpOnly = true;
                return kExitPass;
            }
            else if (arg == "--source-dir") {
                if (!value(opt.sourceDir))
                    return kExitError;
            }
            else if (arg == "--build-dir") {
                if (!value(opt.buildDir))
                    return kExitError;
            }
            else if (arg == "--config") {
                if (!value(opt.configDir))
                    return kExitError;
            }
            else if (arg == "--registry") {
                if (!value(opt.registry))
                    return kExitError;
            }
            else if (arg == "--platform") {
                if (!value(opt.platform))
                    return kExitError;
                if (opt.platform != "macos" && opt.platform != "linux") {
                    std::fprintf(stderr, "archcheck: --platform must be macos or linux\n");
                    return kExitError;
                }
            }
            else if (arg == "--format") {
                if (!value(opt.format))
                    return kExitError;
                if (opt.format != "text" && opt.format != "json") {
                    std::fprintf(stderr, "archcheck: --format must be text or json\n");
                    return kExitError;
                }
            }
            else if (arg == "--emit-allowlist") {
                opt.emitAllowlist = true;
            }
            else if (arg == "--quiet") {
                opt.quiet = true;
            }
            else {
                std::fprintf(stderr, "archcheck: unknown option %s\n", argv[i]);
                printUsage(stderr);
                return kExitError;
            }
        }

        if (opt.sourceDir.empty() || opt.buildDir.empty() || opt.configDir.empty()) {
            std::fprintf(stderr, "archcheck: --source-dir, --build-dir and --config are required\n");
            printUsage(stderr);
            return kExitError;
        }
        if (opt.registry.empty())
            opt.registry = (fs::path(opt.sourceDir) / "DEVNOTES_DETAILS" / "DOMAIN_BOUNDARIES.md").string();
        if (opt.platform.empty()) {
#if defined(__APPLE__)
            opt.platform = "macos";
#else
            opt.platform = "linux";
#endif
        }
        return kExitPass;
    }

    bool readFile(const fs::path &p, std::string &out) {
        std::ifstream f(p, std::ios::binary);
        if (!f)
            return false;
        std::ostringstream ss;
        ss << f.rdbuf();
        out = ss.str();
        return true;
    }

    // Loads one data file through its parser; prints errors and returns false.
    template <class T, class Parser>
    bool load(const fs::path &path, Parser parse, T &out) {
        std::string text;
        if (!readFile(path, text)) {
            std::fprintf(stderr, "archcheck: cannot read %s\n", path.string().c_str());
            return false;
        }
        auto parsed = parse(text, path.string());
        for (const Diagnostic &d : parsed.errors)
            std::fprintf(stderr, "archcheck: %s\n", d.str().c_str());
        if (!parsed.ok())
            return false;
        out = std::move(*parsed.value);
        return true;
    }

    bool isScanned(const fs::path &p) {
        static const std::set<std::string> kExt = {".c", ".cc", ".cpp", ".h", ".hpp", ".y", ".l"};
        return kExt.count(p.extension().string()) != 0;
    }

    bool isHeader(std::string_view path) {
        return path.ends_with(".h") || path.ends_with(".hpp");
    }

    // A domain proper: not Core, Infra or Consumer.
    bool isDomain(Class c) {
        return c != Class::Core && c != Class::Infra && c != Class::Consumer;
    }

} // namespace

int main(int argc, char **argv) {
    Options opt;
    bool helpOnly = false;
    if (const int rc = parseArgs(argc, argv, opt, helpOnly); rc != kExitPass)
        return rc;
    if (helpOnly) {
        printUsage(stdout);
        return kExitPass;
    }
    const Platform platform = opt.platform == "macos" ? Platform::MacOS : Platform::Linux;

    // --- Configuration (exit 2 on any error) --------------------------------
    const fs::path config(opt.configDir);
    DomainMap map;
    RuleTable rules;
    std::vector<AllowlistEntry> allowlist;
    std::map<std::string, MetricCounts> baseline;
    std::map<std::string, RegistryEntry> registry;
    if (!load(config / "domain-map.txt", parseDomainMap, map) ||
        !load(config / "domain-rules.txt", parseRuleTable, rules) ||
        !load(config / "allowlist.txt", parseAllowlist, allowlist) ||
        !load(config / "metrics-baseline.txt", parseMetricsBaseline, baseline) ||
        !load(fs::path(opt.registry), parseRegistry, registry))
        return kExitError;

    const PathMapper mapper(opt.sourceDir, opt.buildDir);

    // Every scanned file under <source-dir>/src/ must classify (FR-002).
    std::map<std::string, Class> classOfFile;
    {
        std::vector<std::string> unclassified;
        const fs::path src = fs::path(mapper.sourceDir()) / "src";
        std::error_code ec;
        for (auto it = fs::recursive_directory_iterator(src, fs::directory_options::skip_permission_denied, ec);
             !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
            if (!it->is_regular_file() || !isScanned(it->path()))
                continue;
            const std::string rel = "src/" + fs::relative(it->path(), src).generic_string();
            if (const auto c = map.classify(rel))
                classOfFile.emplace(rel, *c);
            else
                unclassified.push_back(rel);
        }
        if (!unclassified.empty()) {
            std::sort(unclassified.begin(), unclassified.end());
            for (const std::string &f : unclassified)
                std::fprintf(stderr, "unclassified: %s\n", f.c_str());
            std::fprintf(stderr, "archcheck: %zu unclassified file(s); add rules for them to %s\n",
                         unclassified.size(), (config / "domain-map.txt").string().c_str());
            return kExitError;
        }
    }

    // Generated sources classify through @build/ rules; anything else under
    // the build tree, and everything outside <source-dir>/src/, is out of scope.
    auto classOf = [&](const std::string &mapped) -> std::optional<Class> {
        if (const auto it = classOfFile.find(mapped); it != classOfFile.end())
            return it->second;
        if (mapped.rfind("@build/", 0) == 0 || mapped.rfind("src/", 0) == 0)
            return map.classify(mapped);
        return std::nullopt;
    };
    const ScopeFn inScope = [&](const std::string &mapped) { return classOf(mapped).has_value(); };

    // --- Build tree (exit 2 when incomplete) --------------------------------
    const BuildTree tree = loadBuildTree(mapper, inScope);
    if (!tree.error.empty()) {
        std::fprintf(stderr, "archcheck: %s\n", tree.error.c_str());
        return kExitError;
    }

    // --- Edges --------------------------------------------------------------
    std::vector<Edge> edges;
    const IncludeResolution inc = resolveIncludes(tree, mapper, inScope);
    for (const auto &[from, to] : inc.edges) {
        Edge e;
        e.kind = EdgeKind::Include;
        e.from = from;
        e.to = to;
        e.line = inc.lines.at({from, to});
        e.fromClass = *classOf(from);
        e.toClasses = {*classOf(to)};
        edges.push_back(std::move(e));
    }

    std::vector<std::pair<std::string, std::string>> objects;
    std::set<std::string> sources;
    for (const TranslationUnit &u : tree.units) {
        objects.emplace_back(u.object, u.source);
        sources.insert(u.source);
    }
#if defined(__APPLE__)
    const bool stripUnderscore = true;
#else
    const bool stripUnderscore = false;
#endif
    const SymbolTable symbols = scanSymbols(tree.nm, objects, stripUnderscore);
    if (!symbols.error.empty()) {
        std::fprintf(stderr, "archcheck: %s\n", symbols.error.c_str());
        return kExitError;
    }
    const std::size_t includeEdges = edges.size();
    for (LinkEdge &l : attributeLinkEdges(symbols, classOf)) {
        Edge e;
        e.kind = EdgeKind::Link;
        e.from = l.from;
        e.to = l.symbol;
        e.display = demangle(l.symbol);
        e.fromClass = *classOf(l.from);
        e.toClasses = std::move(l.toClasses);
        e.definedIn = std::move(l.definedIn);
        edges.push_back(std::move(e));
    }

    // --- Classification -----------------------------------------------------
    std::vector<bool> matched;
    classifyEdges(edges, rules, allowlist, platform, matched);

    // Bootstrap aid (research R7): every non-allowed edge as an allowlist
    // line, keeping the id of an entry that already covers it.
    if (opt.emitAllowlist) {
        std::vector<const Edge *> listed;
        for (const Edge &e : edges)
            if (e.status != EdgeStatus::Allowed)
                listed.push_back(&e);
        std::sort(listed.begin(), listed.end(), [](const Edge *a, const Edge *b) {
            return std::tie(a->kind, a->from, a->to) < std::tie(b->kind, b->from, b->to);
        });
        for (const Edge *e : listed) {
            const std::string id = e->status == EdgeStatus::Allowlisted ? e->registryId : "DB-???";
            std::printf("%-7s  %-7s  %s  ->  %s", id.c_str(), e->kind == EdgeKind::Include ? "include" : "link",
                        e->from.c_str(), e->to.c_str());
            if (e->kind == EdgeKind::Link)
                std::printf("   # %s", e->display.c_str());
            std::printf("\n");
        }
        return kExitPass;
    }

    Report report;
    report.platform = opt.platform;
    report.objects = static_cast<long>(objects.size());
    report.sources = static_cast<long>(sources.size());
    report.includeEdges = static_cast<long>(includeEdges);
    report.linkEdges = static_cast<long>(edges.size() - includeEdges);

    for (const Edge &e : edges) {
        if (e.status == EdgeStatus::Violation) {
            Violation v;
            v.kind = e.kind;
            v.from = e.from;
            v.to = e.to;
            v.display = e.display;
            v.fromClass = e.fromClass;
            v.toClasses = e.toClasses;
            v.rule = ruleText(rules, e.fromClass);
            v.definedIn = e.definedIn;
            v.line = e.line;
            report.violations.push_back(std::move(v));
        }
        else if (e.status == EdgeStatus::Allowlisted) {
            report.allowlisted[e.registryId]++;
        }
        // Consumers may depend on anything; their reach into a domain's
        // headers is listed for the record (research R6).
        if (e.fromClass == Class::Consumer && e.kind == EdgeKind::Include && isDomain(e.toClasses.front()))
            report.consumerReach.push_back({e.from, e.to, e.toClasses.front()});
    }
    report.cycles = findCycles(edges);

    const AllowlistCheck entries =
        checkAllowlist(allowlist, matched, edges, registry, platform, inc.seen, sources, inc.inactive);
    report.stale = entries.stale;
    report.badIds = entries.badIds;
    report.notBuilt = entries.notBuilt;

    for (const auto &[file, cls] : classOfFile)
        if (isHeader(file) && !inc.seen.count(file))
            report.unresolvedHeaders.push_back(file);

    // Renderer-core usage per class (FR-012). Only files under src/ are read:
    // generated parsers count through their .y/.l grammars, never twice.
    {
        std::map<std::string, CoreUsage> usage;
        CoreUsage total;
        for (const auto &[file, cls] : classOfFile) {
            if (cls == Class::Core || cls == Class::Consumer)
                continue;
            std::string text;
            if (!readFile(fs::path(mapper.sourceDir()) / file, text))
                continue;
            CoreUsage &u = usage[std::string(className(cls))];
            CoreUsage one;
            countCoreUsage(stripCommentsAndLiterals(text), one);
            u.refs += one.refs;
            u.members.insert(one.members.begin(), one.members.end());
            total.refs += one.refs;
            total.members.insert(one.members.begin(), one.members.end());
        }
        auto row = [&](const std::string &name, long refs, long distinct) {
            MetricRow m;
            m.cls = name;
            m.refs = refs;
            m.distinct = distinct;
            if (const auto b = baseline.find(name); b != baseline.end())
                m.baseline = b->second;
            if (refs != 0 || distinct != 0 || m.baseline)
                report.metrics.push_back(std::move(m));
        };
        for (const auto &[name, u] : usage)
            row(name, u.refs, static_cast<long>(u.members.size()));
        for (const auto &[name, b] : baseline)
            if (name != "TOTAL" && !usage.count(name))
                row(name, 0, 0);
        row("TOTAL", total.refs, static_cast<long>(total.members.size()));
    }

    if (opt.format == "json")
        writeJson(report, stdout);
    else
        writeText(report, stdout, opt.quiet);
    return report.exitCode();
}
