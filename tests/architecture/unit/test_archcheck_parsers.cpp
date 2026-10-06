/**
 * Project: openRender
 *
 * File: test_archcheck_parsers.cpp
 *
 * Description:
 *   Unit tests for archcheck's parsers (spec 022 T007): domain map, rule
 *   table, allowlist lines, registry headings, metric baseline, depfiles and
 *   #include extraction. Inputs are inline strings; every error must carry
 *   "label:line". Formats: specs/022-domain-dependency-guardrails/contracts/
 *   data-files.md. The depfile and registry samples copy the shape of the
 *   real build tree and DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

#include "config.h"
#include "includes.h"

using namespace archcheck;

// ---------------------------------------------------------------------------
// Minimal test framework (the style of the other unit suites)
// ---------------------------------------------------------------------------

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name)                              \
    void test_##name();                         \
    void run_test_##name() {                    \
        printf("Running test: %s ... ", #name); \
        fflush(stdout);                         \
        int failedBefore = tests_failed;        \
        test_##name();                          \
        if (tests_failed == failedBefore) {     \
            tests_passed++;                     \
            printf("PASSED\n");                 \
        }                                       \
    }                                           \
    void test_##name()

#define ASSERT(condition)                                          \
    do {                                                           \
        if (!(condition)) {                                        \
            printf("\nAssertion failed: %s\nFile: %s, Line: %d\n", \
                   #condition, __FILE__, __LINE__);                \
            tests_failed++;                                        \
            return;                                                \
        }                                                          \
    } while (0)

#define RUN(name) run_test_##name()

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// True when some error is reported at `label:line` and its message contains
// `needle`. Negative tests use this so a parser that rejects everything (or
// a stub) cannot pass them.
template <class T>
static bool hasError(const Parsed<T> &r, std::string_view label, int line, std::string_view needle) {
    const std::string where = std::string(label) + ":" + std::to_string(line) + ":";
    for (const Diagnostic &d : r.errors) {
        const std::string s = d.str();
        if (s.rfind(where, 0) == 0 && s.find(needle) != std::string::npos)
            return true;
    }
    if (!r.errors.empty())
        printf("\n  errors were:");
    for (const Diagnostic &d : r.errors)
        printf("\n    %s", d.str().c_str());
    return false;
}

static bool hasDep(const DepfileRule &rule, std::string_view dep) {
    for (const std::string &d : rule.deps)
        if (d == dep)
            return true;
    return false;
}

// ===========================================================================
// (a) domain-map.txt
// ===========================================================================

static const char *kMap = "# comment line\n"
                          "\n"
                          "prefix  src/ri/                         Core\n"
                          "prefix  src/ri/geometry/                Geometry   # trailing comment\n"
                          "file    src/ri/geometry/dlo.h           Procedures\n"
                          "prefix  src/ri/hcshader.                Core\n"
                          "file    @build/src/ri/rib_generated.cpp SceneInterface\n"
                          "prefix  @build/src/libshader/           Shading\n";

TEST(map_parses_all_rules) {
    const auto r = parseDomainMap(kMap, "map.txt");
    ASSERT(r.ok());
    ASSERT(r.value->rules.size() == 6);
    ASSERT(r.value->rules[2].kind == MapKind::File);
    ASSERT(r.value->rules[2].pattern == "src/ri/geometry/dlo.h");
    ASSERT(r.value->rules[2].cls == Class::Procedures);
    ASSERT(r.value->rules[2].line == 5);
}

TEST(map_file_rule_beats_prefix_rule) {
    const auto r = parseDomainMap(kMap, "map.txt");
    ASSERT(r.ok());
    ASSERT(r.value->classify("src/ri/geometry/dlo.h") == Class::Procedures);
    ASSERT(r.value->classify("src/ri/geometry/dlo.cpp") == Class::Geometry);
}

TEST(map_longest_prefix_wins) {
    const auto r = parseDomainMap(kMap, "map.txt");
    ASSERT(r.ok());
    ASSERT(r.value->classify("src/ri/geometry/object.h") == Class::Geometry);
    ASSERT(r.value->classify("src/ri/render/renderer.h") == Class::Core);
    // A prefix need not end at a directory boundary (src/ri/hcshader.*).
    ASSERT(r.value->classify("src/ri/hcshader.cpp") == Class::Core);
}

TEST(map_unmatched_path_is_unclassified) {
    const auto r = parseDomainMap(kMap, "map.txt");
    ASSERT(r.ok());
    ASSERT(!r.value->classify("src/common/algebra.h").has_value());
    ASSERT(!r.value->classify("src/r").has_value());
}

TEST(map_build_paths) {
    const auto r = parseDomainMap(kMap, "map.txt");
    ASSERT(r.ok());
    ASSERT(r.value->classify("@build/src/ri/rib_generated.cpp") == Class::SceneInterface);
    ASSERT(r.value->classify("@build/src/libshader/compiler/rslo_generated.cpp") == Class::Shading);
    // @build/ rules never match the source tree, and vice versa.
    ASSERT(!r.value->classify("src/libshader/compiler/rslo.cpp").has_value());
    ASSERT(r.value->classify("@build/src/ri/render/foo.cpp") != Class::Core);
}

TEST(map_duplicate_pattern_is_error) {
    const auto r = parseDomainMap("prefix src/ri/state/ GraphicsState\n"
                                  "file   src/ri/a.h        Core\n"
                                  "prefix src/ri/state/ Geometry\n",
                                  "map.txt");
    ASSERT(!r.ok());
    ASSERT(hasError(r, "map.txt", 3, "duplicate"));
}

TEST(map_unknown_class_is_error) {
    const auto r = parseDomainMap("prefix src/ri/state/ GraphicsState\n"
                                  "prefix src/ri/x/     Lighting\n",
                                  "map.txt");
    ASSERT(!r.ok());
    ASSERT(hasError(r, "map.txt", 2, "Lighting"));
}

TEST(map_unknown_kind_and_bad_field_count_are_errors) {
    const auto r = parseDomainMap("glob  src/ri/*.h  Core\n"
                                  "prefix src/ri/state/\n",
                                  "map.txt");
    ASSERT(!r.ok());
    ASSERT(hasError(r, "map.txt", 1, "glob"));
    ASSERT(hasError(r, "map.txt", 2, "field"));
}

// ===========================================================================
// (b) domain-rules.txt
// ===========================================================================

static const char *kRules = "# Mirrors constitution v1.2.0\n"
                            "Core           -> *\n"
                            "Consumer       -> *\n"
                            "*              -> Infra\n"
                            "SceneInterface -> GraphicsState Geometry\n"
                            "Geometry       -> GraphicsState Procedures\n"
                            "Shading        -> GraphicsState Texturing ShaderDSO\n"
                            "Hiders         -> GraphicsState Geometry Shading Display\n"
                            "GraphicsState  ->\n"
                            "Texturing      ->\n"
                            "ShaderDSO      ->\n"
                            "Procedures     ->\n"
                            "Display        ->\n";

TEST(class_names_round_trip) {
    for (std::size_t i = 0; i < kClassCount; i++) {
        const Class c = static_cast<Class>(i);
        ASSERT(classFromName(className(c)) == c);
    }
    ASSERT(classFromName("GraphicsState") == Class::GraphicsState);
    ASSERT(!classFromName("graphicsstate").has_value());
    ASSERT(!classFromName("*").has_value());
}

TEST(rules_listed_edges_allowed) {
    const auto r = parseRuleTable(kRules, "rules.txt");
    ASSERT(r.ok());
    ASSERT(r.value->allows(Class::Hiders, Class::Shading));
    ASSERT(r.value->allows(Class::Shading, Class::Texturing));
    ASSERT(r.value->allows(Class::Geometry, Class::Procedures));
}

TEST(rules_unlisted_edges_denied) {
    const auto r = parseRuleTable(kRules, "rules.txt");
    ASSERT(r.ok());
    ASSERT(!r.value->allows(Class::Texturing, Class::Hiders));
    ASSERT(!r.value->allows(Class::GraphicsState, Class::Shading));
    ASSERT(!r.value->allows(Class::Geometry, Class::Core));
    ASSERT(!r.value->allows(Class::Shading, Class::Hiders));
    ASSERT(!r.value->allows(Class::Infra, Class::Geometry));
    ASSERT(!r.value->allows(Class::Display, Class::Core));
}

TEST(rules_wildcards) {
    const auto r = parseRuleTable(kRules, "rules.txt");
    ASSERT(r.ok());
    for (std::size_t i = 0; i < kClassCount; i++) {
        const Class c = static_cast<Class>(i);
        ASSERT(r.value->allows(Class::Core, c));     // Core -> *
        ASSERT(r.value->allows(Class::Consumer, c)); // Consumer -> *
        ASSERT(r.value->allows(c, Class::Infra));    // * -> Infra
    }
}

TEST(rules_same_class_always_allowed) {
    // Even a class whose row is empty, or that has no row at all.
    const auto r = parseRuleTable("GraphicsState ->\n", "rules.txt");
    ASSERT(r.ok());
    for (std::size_t i = 0; i < kClassCount; i++) {
        const Class c = static_cast<Class>(i);
        ASSERT(r.value->allows(c, c));
    }
}

TEST(rules_cyclic_domain_subgraph_is_error) {
    const auto r = parseRuleTable("Shading   -> Texturing\n"
                                  "Texturing -> Geometry\n"
                                  "Geometry  -> Shading\n",
                                  "rules.txt");
    ASSERT(!r.ok());
    ASSERT(hasError(r, "rules.txt", 3, "cycle"));
}

TEST(rules_core_and_consumer_wildcards_are_not_cycles) {
    // Core -> * and Consumer -> * reach every class, Core included, and
    // * -> Infra reaches Infra from everything; none of it is a cycle.
    const auto r = parseRuleTable("Core -> *\nConsumer -> *\n* -> Infra\nShading -> Texturing\n",
                                  "rules.txt");
    ASSERT(r.ok());
    ASSERT(r.value->allows(Class::Infra, Class::Infra));
}

TEST(rules_infra_to_domain_is_error) {
    const auto r = parseRuleTable("* -> Infra\nInfra -> Geometry\n", "rules.txt");
    ASSERT(!r.ok());
    ASSERT(hasError(r, "rules.txt", 2, "Infra"));
}

TEST(rules_infra_to_core_is_error) {
    const auto r = parseRuleTable("Infra -> Core\n", "rules.txt");
    ASSERT(!r.ok());
    ASSERT(hasError(r, "rules.txt", 1, "Infra"));
}

TEST(rules_unknown_class_and_missing_arrow_are_errors) {
    const auto r = parseRuleTable("Shading -> Lighting\n"
                                  "Hiders Geometry\n",
                                  "rules.txt");
    ASSERT(!r.ok());
    ASSERT(hasError(r, "rules.txt", 1, "Lighting"));
    ASSERT(hasError(r, "rules.txt", 2, "->"));
}

// ===========================================================================
// (c) allowlist.txt
// ===========================================================================

TEST(allowlist_parses_include_and_link_entries) {
    const auto r = parseAllowlist(
        "# header\n"
        "DB-004   include  src/ri/geometry/object.h  ->  src/ri/render/renderer.h\n"
        "DB-001   link     src/libshader/shading/shaderPl.cpp -> _ZN9CRenderer9getFilterEPKc"
        "   # CRenderer::getFilter(char const*)\n"
        "DB-0123  link     src/ri/hiders/foo.cpp  ->  _ZN3FooC1Ev   macos\n"
        "DB-005   include  src/ri/hiders/bar.cpp ->  src/ri/render/renderer.h  linux\n",
        "allow.txt");
    ASSERT(r.ok());
    const auto &e = *r.value;
    ASSERT(e.size() == 4);

    ASSERT(e[0].registryId == "DB-004");
    ASSERT(e[0].kind == EdgeKind::Include);
    ASSERT(e[0].fromFile == "src/ri/geometry/object.h");
    ASSERT(e[0].to == "src/ri/render/renderer.h");
    ASSERT(!e[0].platform.has_value());
    ASSERT(e[0].line == 2);

    // The trailing comment is not a platform tag.
    ASSERT(e[1].kind == EdgeKind::Link);
    ASSERT(e[1].to == "_ZN9CRenderer9getFilterEPKc");
    ASSERT(!e[1].platform.has_value());

    ASSERT(e[2].registryId == "DB-0123");
    ASSERT(e[2].platform == Platform::MacOS);
    ASSERT(e[3].platform == Platform::Linux);
}

TEST(allowlist_bad_id_is_error) {
    const auto r = parseAllowlist("DB-12   include a.h -> b.h\n"
                                  "DB12    include a.h -> c.h\n"
                                  "DB-abc  include a.h -> d.h\n",
                                  "allow.txt");
    ASSERT(!r.ok());
    ASSERT(hasError(r, "allow.txt", 1, "DB-12"));
    ASSERT(hasError(r, "allow.txt", 2, "DB12"));
    ASSERT(hasError(r, "allow.txt", 3, "DB-abc"));
}

TEST(allowlist_bad_kind_tag_and_shape_are_errors) {
    const auto r = parseAllowlist("DB-004 symbol  a.cpp -> _Z1fv\n"
                                  "DB-004 include a.h   -> b.h  windows\n"
                                  "DB-004 include a.h      b.h\n",
                                  "allow.txt");
    ASSERT(!r.ok());
    ASSERT(hasError(r, "allow.txt", 1, "symbol"));
    ASSERT(hasError(r, "allow.txt", 2, "windows"));
    ASSERT(hasError(r, "allow.txt", 3, "->"));
}

TEST(allowlist_duplicate_entry_is_error) {
    const auto r = parseAllowlist("DB-004 include a.h -> b.h macos\n"
                                  "DB-004 include a.h -> c.h\n"
                                  "DB-005 include a.h -> b.h macos\n",
                                  "allow.txt");
    ASSERT(!r.ok());
    ASSERT(hasError(r, "allow.txt", 3, "duplicate"));
}

TEST(allowlist_same_edge_on_both_platforms_is_fine) {
    const auto r = parseAllowlist("DB-004 link a.cpp -> _Z1fv macos\n"
                                  "DB-004 link a.cpp -> _Z1fv linux\n",
                                  "allow.txt");
    ASSERT(r.ok());
    ASSERT(r.value->size() == 2);
}

TEST(allowlist_tagged_and_untagged_same_edge_is_error) {
    const auto r = parseAllowlist("DB-004 link a.cpp -> _Z1fv\n"
                                  "DB-004 link a.cpp -> _Z1gv\n"
                                  "DB-004 link a.cpp -> _Z1fv linux\n",
                                  "allow.txt");
    ASSERT(!r.ok());
    ASSERT(hasError(r, "allow.txt", 3, "tagged"));
}

// ===========================================================================
// (d) registry headings
// ===========================================================================

static const char *kRegistry = "# Domain boundaries\n"
                               "\n"
                               "Intro text mentioning ### DB-900 inline is not a heading.\n"
                               "\n"
                               "## Open\n"
                               "\n"
                               "### DB-001 \xE2\x80\x94 Shading resolves renderer-core symbols at load time\n"
                               "Body.\n"
                               "### DB-0042 \xE2\x80\x94 Four-digit id\n"
                               "#### DB-777 \xE2\x80\x94 a level-4 heading is not an entry\n"
                               "### DB-01 \xE2\x80\x94 too few digits, not an entry\n"
                               "### DB-002a \xE2\x80\x94 no word boundary, not an entry\n"
                               "\n"
                               "## Resolved\n"
                               "\n"
                               "### DB-003 \xE2\x80\x94 Fixed long ago\n";

TEST(registry_open_before_resolved_heading) {
    const auto r = parseRegistry(kRegistry, "REG.md");
    ASSERT(r.ok());
    const auto &m = *r.value;
    ASSERT(m.size() == 3);
    ASSERT(m.count("DB-001") == 1);
    ASSERT(m.at("DB-001").status == RegistryStatus::Open);
    ASSERT(m.at("DB-001").line == 7);
    ASSERT(m.count("DB-0042") == 1);
    ASSERT(m.at("DB-0042").status == RegistryStatus::Open);
    ASSERT(m.count("DB-003") == 1);
    ASSERT(m.at("DB-003").status == RegistryStatus::Resolved);
}

TEST(registry_non_entries_ignored) {
    const auto r = parseRegistry(kRegistry, "REG.md");
    ASSERT(r.ok());
    ASSERT(r.value->count("DB-900") == 0);
    ASSERT(r.value->count("DB-777") == 0);
    ASSERT(r.value->count("DB-01") == 0);
    ASSERT(r.value->count("DB-002") == 0);
    ASSERT(r.value->count("DB-002a") == 0);
}

TEST(registry_without_resolved_section_is_all_open) {
    const auto r = parseRegistry("### DB-010 \xE2\x80\x94 a\n### DB-011 \xE2\x80\x94 b\n", "REG.md");
    ASSERT(r.ok());
    ASSERT(r.value->size() == 2);
    ASSERT(r.value->at("DB-011").status == RegistryStatus::Open);
}

TEST(registry_duplicate_id_is_error) {
    const auto r = parseRegistry("### DB-010 \xE2\x80\x94 a\n"
                                 "## Resolved\n"
                                 "### DB-010 \xE2\x80\x94 again\n",
                                 "REG.md");
    ASSERT(!r.ok());
    ASSERT(hasError(r, "REG.md", 3, "duplicate"));
}

// ===========================================================================
// (e) metrics-baseline.txt
// ===========================================================================

TEST(baseline_parses_classes_and_total) {
    const auto r = parseMetricsBaseline("# <class> <refs> <distinct>\n"
                                        "Geometry   88  24\n"
                                        "Hiders    611  57   # comment\n"
                                        "TOTAL     699  70\n",
                                        "base.txt");
    ASSERT(r.ok());
    const auto &m = *r.value;
    ASSERT(m.size() == 3);
    ASSERT(m.at("Geometry").refs == 88);
    ASSERT(m.at("Geometry").distinct == 24);
    ASSERT(m.at("Hiders").refs == 611);
    ASSERT(m.at("TOTAL").distinct == 70);
}

TEST(baseline_errors) {
    const auto r = parseMetricsBaseline("Lighting  1  1\n"
                                        "Geometry  x  1\n"
                                        "Hiders    -3 1\n"
                                        "Shading   5\n"
                                        "Infra     1  1\n"
                                        "Infra     2  2\n",
                                        "base.txt");
    ASSERT(!r.ok());
    ASSERT(hasError(r, "base.txt", 1, "Lighting"));
    ASSERT(hasError(r, "base.txt", 2, "x"));
    ASSERT(hasError(r, "base.txt", 3, "-3"));
    ASSERT(hasError(r, "base.txt", 4, "field"));
    ASSERT(hasError(r, "base.txt", 6, "duplicate"));
}

// ===========================================================================
// (f) depfiles
// ===========================================================================

TEST(depfile_real_makefiles_shape) {
    // The shape the Makefiles generator writes (clang -MD -MT obj -MF obj.d):
    // build-relative target, absolute deps, one per continuation line.
    const auto r = parseDepfile(
        "src/ri/CMakeFiles/ribVector_obj.dir/state/options.cpp.o: \\\n"
        "  /Volumes/P/openrender/src/ri/state/options.cpp \\\n"
        "  /Applications/Xcode.app/SDKs/MacOSX.sdk/usr/include/c++/v1/math.h \\\n"
        "  /Volumes/P/openrender/src/ri/core/stats.h \\\n"
        "  /Volumes/P/openrender/src/ri/state/attributes.h\n",
        "options.cpp.o.d");
    ASSERT(r.ok());
    ASSERT(r.value->size() == 1);
    const DepfileRule &rule = (*r.value)[0];
    ASSERT(rule.targets.size() == 1);
    ASSERT(rule.targets[0] == "src/ri/CMakeFiles/ribVector_obj.dir/state/options.cpp.o");
    ASSERT(rule.deps.size() == 4);
    ASSERT(rule.deps[0] == "/Volumes/P/openrender/src/ri/state/options.cpp");
    ASSERT(rule.deps[3] == "/Volumes/P/openrender/src/ri/state/attributes.h");
}

TEST(depfile_several_deps_per_line_and_escaped_spaces) {
    const auto r = parseDepfile("a.o: /x/My\\ Dir/a.cpp /x/b.h \\\n"
                                "   /x/c.h\n",
                                "a.o.d");
    ASSERT(r.ok());
    const DepfileRule &rule = (*r.value)[0];
    ASSERT(rule.deps.size() == 3);
    ASSERT(rule.deps[0] == "/x/My Dir/a.cpp");
    ASSERT(hasDep(rule, "/x/b.h"));
    ASSERT(hasDep(rule, "/x/c.h"));
}

TEST(depfile_multiple_targets_and_rules) {
    // Multiple targets on one rule, and -MP style phony rules that add no deps.
    const auto r = parseDepfile("a.o a.d: /x/a.cpp /x/a.h\n"
                                "\n"
                                "/x/a.h:\n",
                                "a.o.d");
    ASSERT(r.ok());
    ASSERT(r.value->size() == 2);
    ASSERT((*r.value)[0].targets.size() == 2);
    ASSERT((*r.value)[0].targets[1] == "a.d");
    ASSERT((*r.value)[0].deps.size() == 2);
    ASSERT((*r.value)[1].targets[0] == "/x/a.h");
    ASSERT((*r.value)[1].deps.empty());
}

TEST(depfile_missing_colon_is_error) {
    const auto r = parseDepfile("a.o /x/a.cpp\n", "a.o.d");
    ASSERT(!r.ok());
    ASSERT(hasError(r, "a.o.d", 1, "':'"));
}

// ===========================================================================
// (g) #include extraction
// ===========================================================================

TEST(includes_quoted_and_angled) {
    const auto inc = extractIncludes("#include \"common/os.h\"\n"
                                     "#include <vector>\n"
                                     "  #  include   \"renderer.h\"  // trailing comment\n");
    ASSERT(inc.size() == 3);
    ASSERT(inc[0].name == "common/os.h");
    ASSERT(!inc[0].angled);
    ASSERT(inc[0].line == 1);
    ASSERT(inc[1].name == "vector");
    ASSERT(inc[1].angled);
    ASSERT(inc[2].name == "renderer.h");
    ASSERT(inc[2].line == 3);
}

TEST(includes_in_comments_skipped) {
    const auto inc = extractIncludes("// #include \"a.h\"\n"
                                     "/* #include \"b.h\"\n"
                                     "#include \"c.h\" */\n"
                                     "#include \"d.h\" /* after */\n"
                                     "/* one-line */ #include \"e.h\"\n");
    ASSERT(inc.size() == 2);
    ASSERT(inc[0].name == "d.h");
    ASSERT(inc[0].line == 4);
    ASSERT(inc[1].name == "e.h");
    ASSERT(inc[1].line == 5);
}

TEST(includes_conditionals_left_to_depfile) {
    // Both branches are extracted; resolution against the depfile decides.
    const auto inc = extractIncludes("#ifdef _WIN32\n"
                                     "#include \"win.h\"\n"
                                     "#else\n"
                                     "#include \"posix.h\"\n"
                                     "#endif\n"
                                     "#if 0\n"
                                     "#include \"dead.h\"\n"
                                     "#endif\n");
    ASSERT(inc.size() == 3);
    ASSERT(inc[0].name == "win.h");
    ASSERT(inc[1].name == "posix.h");
    ASSERT(inc[2].name == "dead.h");
}

TEST(includes_ignores_non_directives) {
    const auto inc = extractIncludes("#define INC \"x.h\"\n"
                                     "#include INC\n"
                                     "const char *s = \"#include \\\"s.h\\\"\";\n"
                                     "#includex \"y.h\"\n"
                                     "#include_next <z.h>\n"
                                     "#include \"ok.h\"\n");
    ASSERT(inc.size() == 1);
    ASSERT(inc[0].name == "ok.h");
    ASSERT(inc[0].line == 6);
}

int main() {
    // (a) domain map
    RUN(map_parses_all_rules);
    RUN(map_file_rule_beats_prefix_rule);
    RUN(map_longest_prefix_wins);
    RUN(map_unmatched_path_is_unclassified);
    RUN(map_build_paths);
    RUN(map_duplicate_pattern_is_error);
    RUN(map_unknown_class_is_error);
    RUN(map_unknown_kind_and_bad_field_count_are_errors);
    // (b) rule table
    RUN(class_names_round_trip);
    RUN(rules_listed_edges_allowed);
    RUN(rules_unlisted_edges_denied);
    RUN(rules_wildcards);
    RUN(rules_same_class_always_allowed);
    RUN(rules_cyclic_domain_subgraph_is_error);
    RUN(rules_core_and_consumer_wildcards_are_not_cycles);
    RUN(rules_infra_to_domain_is_error);
    RUN(rules_infra_to_core_is_error);
    RUN(rules_unknown_class_and_missing_arrow_are_errors);
    // (c) allowlist
    RUN(allowlist_parses_include_and_link_entries);
    RUN(allowlist_bad_id_is_error);
    RUN(allowlist_bad_kind_tag_and_shape_are_errors);
    RUN(allowlist_duplicate_entry_is_error);
    RUN(allowlist_same_edge_on_both_platforms_is_fine);
    RUN(allowlist_tagged_and_untagged_same_edge_is_error);
    // (d) registry
    RUN(registry_open_before_resolved_heading);
    RUN(registry_non_entries_ignored);
    RUN(registry_without_resolved_section_is_all_open);
    RUN(registry_duplicate_id_is_error);
    // (e) metric baseline
    RUN(baseline_parses_classes_and_total);
    RUN(baseline_errors);
    // (f) depfiles
    RUN(depfile_real_makefiles_shape);
    RUN(depfile_several_deps_per_line_and_escaped_spaces);
    RUN(depfile_multiple_targets_and_rules);
    RUN(depfile_missing_colon_is_error);
    // (g) #include extraction
    RUN(includes_quoted_and_angled);
    RUN(includes_in_comments_skipped);
    RUN(includes_conditionals_left_to_depfile);
    RUN(includes_ignores_non_directives);
    printf("\n%d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed == 0 ? 0 : 1;
}
