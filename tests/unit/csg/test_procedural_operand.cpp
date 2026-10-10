/**
 * Project: openRender
 *
 * File: test_procedural_operand.cpp
 *
 * Description:
 *   Unit test (T062, spec 021) for procedural operands (FR-024, FR-025,
 *   FR-025a): a Procedural inside a "primitive" block, or directly in a
 *   boolean block (joining its first operand), is captured without error,
 *   left unexpanded at SolidEnd, and expanded once, on the instance's first
 *   use, into its operand -- recursively, with a nested SolidBegin swallowed
 *   and reported once, an empty expansion giving an empty operand, an open
 *   quadric expansion getting implied caps, and expanded objects carrying the
 *   leaf's solid-fragment flag and inside parity.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>

#include "csgSolid.h"
#include "csgTestUtils.h"
#include "delayed.h"
#include "quadrics.h"
#include "common/stats.h"

using namespace csgtest;

static CCSGSolid *solidFrom(CCsgSession &s, const std::function<void()> &emit) {
    return dynamic_cast<CCSGSolid *>(s.one(emit));
}

// Every object under o (o excluded), depth first
static void descendants(CObject *o, std::vector<CObject *> &out) {
    for (CObject *c = o->children; c != NULL; c = c->sibling) {
        out.push_back(c);
        descendants(c, out);
    }
}

template <class T> static int countOf(const std::vector<CObject *> &objects) {
    int n = 0;
    for (CObject *o : objects)
        n += dynamic_cast<T *>(o) != NULL ? 1 : 0;
    return n;
}

// Everything f writes to stdout and stderr
static std::string outputOf(const std::function<void()> &f) {
    fflush(stdout);
    fflush(stderr);
    char path[] = "/tmp/csg_proc_XXXXXX";
    int fd = mkstemp(path);
    int savedOut = dup(1), savedErr = dup(2);
    dup2(fd, 1);
    dup2(fd, 2);
    f();
    fflush(stdout);
    fflush(stderr);
    dup2(savedOut, 1);
    dup2(savedErr, 2);
    close(savedOut);
    close(savedErr);
    lseek(fd, 0, SEEK_SET);
    std::string out;
    char buf[4096];
    ssize_t n;
    while ((n = read(fd, buf, sizeof(buf))) > 0)
        out.append(buf, (size_t)n);
    close(fd);
    unlink(path);
    return out;
}

static int occurrences(const std::string &text, const std::string &what) {
    int n = 0;
    for (size_t at = text.find(what); at != std::string::npos; at = text.find(what, at + 1))
        n++;
    return n;
}

// ---------------------------------------------------------------------------
// Archives, written to a scratch directory for the test's lifetime
// ---------------------------------------------------------------------------

static std::string scratchDir() {
    static std::string dir;
    if (dir.empty()) {
        char tmpl[] = "/tmp/csg_proc_dir_XXXXXX";
        dir = mkdtemp(tmpl);
    }
    return dir;
}

static std::string archive(const char *name, const char *contents) {
    const std::string path = scratchDir() + "/" + name;
    FILE *f = fopen(path.c_str(), "w");
    fputs(contents, f);
    fclose(f);
    return path;
}

// A DelayedReadArchive procedural of path with the given bound, emitted as
// the RIB parser emits one
static void delayedReadArchive(const std::string &path, float lo, float hi) {
    CDelayedData *data = new CDelayedData();
    data->generator = strdup(path.c_str());
    initv(data->bmin, lo, lo, lo);
    initv(data->bmax, hi, hi, hi);
    RtBound bound = {lo, hi, lo, hi, lo, hi};
    RiProcedural(data, bound, RiProcDelayedReadArchive, RiProcFree);
}

static std::string sphereArchive() {
    return archive("sphere.rib", "Sphere 1 -1 1 360\n");
}

static void cylinderBlock() {
    RiSolidBegin(RI_PRIMITIVE);
    RiCylinder(0.4f, -2.0f, 2.0f, 360.0f, RI_NULL);
    RiSolidEnd();
}

static bool isDelayed(const CObject *o) {
    return dynamic_cast<const CDelayedObject *>(o) != NULL;
}

// The definition holds `numOperands` operands, and operand `op`'s template
// is the one captured procedural
static bool capturedProcedural(const CCSGSolid *solid, int numOperands, int op) {
    return solid != NULL && solid->definition->numOperands() == numOperands &&
           solid->definition->operandMembers[op].size() == 1 && isDelayed(solid->definition->operandMembers[op][0]);
}

// No member of any resolved operand is still a procedural
static bool fullyExpanded(const CCSGInstanceState *state) {
    for (const CCSGResolvedOperand *op : state->operands)
        for (const CObject *m : op->members)
            if (isDelayed(m))
                return false;
    return true;
}

// ---------------------------------------------------------------------------

TEST(procedural_in_primitive_is_captured_without_error) {
    CCsgSession s;
    const std::string path = sphereArchive();
    CCSGSolid *solid = NULL;
    const std::string out = outputOf([&] {
        solid = solidFrom(s, [&] {
            RiSolidBegin(RI_DIFFERENCE);
            RiSolidBegin(RI_PRIMITIVE);
            delayedReadArchive(path, -1, 1);
            RiSolidEnd();
            cylinderBlock();
            RiSolidEnd();
        });
    });
    ASSERT(occurrences(out, "cannot be declared inside a SolidBegin block") == 0);
    ASSERT(capturedProcedural(solid, 2, 0));
}

TEST(procedural_directly_in_a_boolean_block_joins_its_first_operand) {
    CCsgSession s;
    const std::string path = sphereArchive();
    const RtToken operations[2] = {RI_UNION, RI_DIFFERENCE};
    for (RtToken operation : operations) {
        CCSGSolid *solid = NULL;
        const std::string out = outputOf([&] {
            solid = solidFrom(s, [&] {
                RiSolidBegin(operation);
                RiTransformBegin();
                RiTranslate(-1.5f, 0, 0);
                RiSphere(1.0f, -1.0f, 1.0f, 360.0f, RI_NULL);
                RiTransformEnd();
                RiTransformBegin();
                RiTranslate(1.5f, 0, 0);
                delayedReadArchive(path, -1, 1);
                RiTransformEnd();
                cylinderBlock();
                RiSolidEnd();
            });
        });
        ASSERT(occurrences(out, "cannot be declared inside a SolidBegin block") == 0);
        ASSERT(solid != NULL && solid->definition->numOperands() == 2);
        // The direct sphere and the procedural form the first operand together
        ASSERT(solid->definition->operandMembers[0].size() == 2);
        ASSERT(countOf<CDelayedObject>(solid->definition->operandMembers[0]) == 1);
        solid->resolve(s.ctx());
        const CCSGInstanceState *state = solid->instanceState();
        ASSERT(state != NULL && fullyExpanded(state));
        ASSERT(countOf<CSphere>(state->operands[0]->members) == 2);
        ASSERT(state->operands[0]->kind == CSG_OPERAND_MULTI_SURFACE); // parity-combined
    }
}

TEST(nothing_is_expanded_at_solid_end) {
    CCsgSession s;
    const std::string path = sphereArchive();
    const int expansions = stats.numCsgProceduralExpansions;
    CCSGSolid *solid = solidFrom(s, [&] {
        RiSolidBegin(RI_DIFFERENCE);
        RiSolidBegin(RI_PRIMITIVE);
        delayedReadArchive(path, -1, 1);
        RiSolidEnd();
        cylinderBlock();
        RiSolidEnd();
    });
    ASSERT(capturedProcedural(solid, 2, 0));
    ASSERT(solid->instanceState() == NULL);
    ASSERT(stats.numCsgProceduralExpansions == expansions);
}

TEST(expanded_once_on_first_use_and_never_when_never_needed) {
    CCsgSession s;
    const std::string path = sphereArchive();
    auto emit = [&] {
        RiSolidBegin(RI_DIFFERENCE);
        RiSolidBegin(RI_PRIMITIVE);
        delayedReadArchive(path, -1, 1);
        RiSolidEnd();
        cylinderBlock();
        RiSolidEnd();
    };
    CCSGSolid *used = solidFrom(s, emit);
    CCSGSolid *neverNeeded = solidFrom(s, emit);
    ASSERT(capturedProcedural(used, 2, 0) && capturedProcedural(neverNeeded, 2, 0));
    const int expansions = stats.numCsgProceduralExpansions;
    used->resolve(s.ctx());
    ASSERT(stats.numCsgProceduralExpansions == expansions + 1);
    used->resolve(s.ctx());
    ASSERT(stats.numCsgProceduralExpansions == expansions + 1);
    // The never-needed solid's procedural was never expanded
    ASSERT(neverNeeded->instanceState() == NULL);
    // It expanded into its operand: real geometry, no procedural left
    const CCSGInstanceState *state = used->instanceState();
    ASSERT(state != NULL && fullyExpanded(state));
    ASSERT(state->operands[0]->members.size() == 1);
    ASSERT(dynamic_cast<CSphere *>(state->operands[0]->members[0]) != NULL);
    ASSERT(state->operands[0]->kind == CSG_OPERAND_SINGLE_QUADRIC);
    std::vector<CObject *> leaves;
    descendants(used, leaves);
    ASSERT(countOf<CSphere>(leaves) == 1);
    ASSERT(countOf<CDelayedObject>(leaves) == 0);
    // The template keeps its procedural
    ASSERT(isDelayed(used->definition->operandMembers[0][0]));
}

TEST(nested_procedurals_expand_recursively) {
    CCsgSession s;
    const std::string inner = sphereArchive();
    const std::string outer = archive("outer.rib", ("Procedural \"DelayedReadArchive\" [\"" + inner + "\"] [-1 1 -1 1 -1 1]\n").c_str());
    CCSGSolid *solid = solidFrom(s, [&] {
        RiSolidBegin(RI_DIFFERENCE);
        RiSolidBegin(RI_PRIMITIVE);
        delayedReadArchive(outer, -1, 1);
        RiSolidEnd();
        cylinderBlock();
        RiSolidEnd();
    });
    ASSERT(capturedProcedural(solid, 2, 0));
    const int expansions = stats.numCsgProceduralExpansions;
    solid->resolve(s.ctx());
    ASSERT(stats.numCsgProceduralExpansions == expansions + 2);
    const CCSGInstanceState *state = solid->instanceState();
    ASSERT(state != NULL && fullyExpanded(state));
    ASSERT(countOf<CSphere>(state->operands[0]->members) == 1);
}

TEST(a_solid_block_inside_an_expansion_is_swallowed_and_reported_once) {
    CCsgSession s;
    const std::string path = archive("nested-solid.rib", "Sphere 1 -1 1 360\n"
                                                          "SolidBegin \"union\"\n"
                                                          "  SolidBegin \"primitive\"\n"
                                                          "    Translate 5 0 0\n"
                                                          "    Sphere 1 -1 1 360\n"
                                                          "  SolidEnd\n"
                                                          "SolidEnd\n");
    CCSGSolid *solid = solidFrom(s, [&] {
        RiSolidBegin(RI_DIFFERENCE);
        RiSolidBegin(RI_PRIMITIVE);
        delayedReadArchive(path, -1, 6);
        RiSolidEnd();
        cylinderBlock();
        RiSolidEnd();
    });
    ASSERT(capturedProcedural(solid, 2, 0));
    const size_t capturedBefore = s.capture()->captured.size();
    const int resolvedBefore = stats.numCsgInstancesResolved;
    const std::string out = outputOf([&] { solid->resolve(s.ctx()); });
    ASSERT(occurrences(out, "cannot be nested inside a \"primitive\" solid block") == 1);
    const CCSGInstanceState *state = solid->instanceState();
    ASSERT(state != NULL && fullyExpanded(state));
    // Only the archive's first sphere: the nested block's contents are absent
    ASSERT(state->operands[0]->members.size() == 1);
    ASSERT(state->operands[0]->bmax[0] < 4.0f);
    // Its SolidEnd resolved nothing and created no solid
    ASSERT(s.capture()->captured.size() == capturedBefore);
    ASSERT(stats.numCsgInstancesResolved == resolvedBefore + 1);
    std::vector<CObject *> leaves;
    descendants(solid, leaves);
    ASSERT(countOf<CCSGSolid>(leaves) == 0);
}

TEST(an_empty_expansion_is_an_empty_operand) {
    CCsgSession s;
    const std::string path = archive("empty.rib", "# nothing\n");
    CCSGSolid *solid = solidFrom(s, [&] {
        RiSolidBegin(RI_DIFFERENCE);
        RiSolidBegin(RI_PRIMITIVE);
        delayedReadArchive(path, -1, 1);
        RiSolidEnd();
        cylinderBlock();
        RiSolidEnd();
    });
    ASSERT(capturedProcedural(solid, 2, 0));
    solid->resolve(s.ctx());
    const CCSGInstanceState *state = solid->instanceState();
    ASSERT(state != NULL);
    ASSERT(state->operands[0]->kind == CSG_OPERAND_EMPTY);
    ASSERT(solid->children == NULL); // difference(empty, B) is empty
}

TEST(the_open_cylinder_archive_gets_implied_caps) {
    CCsgSession s;
    const char *home = getenv("ORENDERHOME");
    ASSERT(home != NULL);
    const std::string path = std::string(home) + "/examples/rib/tests/archives/csg-open-cylinder-operand.rib";
    ASSERT(access(path.c_str(), R_OK) == 0);
    CCSGSolid *solid = solidFrom(s, [&] {
        RiSolidBegin(RI_INTERSECTION);
        RiSolidBegin(RI_PRIMITIVE);
        RiSphere(1.0f, -1.0f, 1.0f, 360.0f, RI_NULL);
        RiSolidEnd();
        RiSolidBegin(RI_PRIMITIVE);
        delayedReadArchive(path, -1.2f, 1.2f);
        RiSolidEnd();
        RiSolidEnd();
    });
    ASSERT(capturedProcedural(solid, 2, 1));
    solid->resolve(s.ctx());
    const CCSGInstanceState *state = solid->instanceState();
    ASSERT(state != NULL && fullyExpanded(state));
    ASSERT(state->operands[1]->kind == CSG_OPERAND_SINGLE_QUADRIC);
    ASSERT(dynamic_cast<CCylinder *>(state->operands[1]->members[0]) != NULL);
    // The half-sweep cylinder's two end caps and two sweep-gap faces
    std::vector<CObject *> leaves;
    descendants(solid, leaves);
    ASSERT(countOf<CDisk>(leaves) == 4);
}

TEST(expanded_objects_carry_the_fragment_flag_and_inside_parity) {
    CCsgSession s;
    const std::string path = sphereArchive();
    CCSGSolid *solid = solidFrom(s, [&] {
        RiSolidBegin(RI_DIFFERENCE);
        cylinderBlock();
        RiSolidBegin(RI_PRIMITIVE);
        delayedReadArchive(path, -1, 1); // the subtrahend: flipped
        RiSolidEnd();
        RiSolidEnd();
    });
    ASSERT(capturedProcedural(solid, 2, 1));
    ASSERT(solid->definition->operandFlip(1) == 1);
    const unsigned int templateInside = solid->definition->operandMembers[1][0]->attributes->flags & ATTRIBUTES_FLAGS_INSIDE;
    solid->resolve(s.ctx());
    const CCSGInstanceState *state = solid->instanceState();
    ASSERT(state != NULL && fullyExpanded(state));
    ASSERT(!state->operands[1]->members.empty());
    for (const CObject *m : state->operands[1]->members) {
        ASSERT(m->attributes->flags & ATTRIBUTES_FLAGS_SOLID_FRAGMENT);
        ASSERT((m->attributes->flags & ATTRIBUTES_FLAGS_INSIDE) != templateInside);
    }
}

int main() {
    RUN(procedural_in_primitive_is_captured_without_error);
    RUN(procedural_directly_in_a_boolean_block_joins_its_first_operand);
    RUN(nothing_is_expanded_at_solid_end);
    RUN(expanded_once_on_first_use_and_never_when_never_needed);
    RUN(nested_procedurals_expand_recursively);
    RUN(a_solid_block_inside_an_expansion_is_swallowed_and_reported_once);
    RUN(an_empty_expansion_is_an_empty_operand);
    RUN(the_open_cylinder_archive_gets_implied_caps);
    RUN(expanded_objects_carry_the_fragment_flag_and_inside_parity);
    SUMMARY();
}
