/*
 * Spec 035, Closure_Layout (label closures): the node layout of contracts/closure-abi.md "Nodes", pinned on LP64 at compile
 * time and again at run time, and the header's version. The compatibility figure with OSL's ClosureComponent is the
 * parameter-block offset (16), analyze E13: sizeof, alignof and the other offsets are PRINTED for spec 049's layout test, which
 * compares them against oslclosure.h, and only the block offset is asserted to be the figure that must match.
 *
 * closureAbi.h is also compiled as strict C17 (layout_c17.c, -std=c17 -Wall -Wextra -Wpedantic -Werror) and the C side reports
 * the same numbers, so the two languages cannot disagree.
 *
 * Written red first: closureAbi.h does not exist, so this does not compile.
 */
#include <cstddef>
#include <cstdint>
#include <cstdio>

#include "closureAbi.h"
#include "closures/test_support.h"

// The compile-time pins (LP64). If the platform is not LP64 the pins are skipped and the run-time checks below still run.
#if defined(__LP64__) || defined(_LP64)
static_assert(sizeof(OrClosureComponent) == 16 && alignof(OrClosureComponent) == 16, "OrClosureComponent is 16 bytes, 16-aligned");
static_assert(offsetof(OrClosureComponent, id) == 0 && offsetof(OrClosureComponent, w) == 4, "OrClosureComponent: id 0, w 4");
static_assert(sizeof(OrClosureMul) == 24 && alignof(OrClosureMul) == 8, "OrClosureMul is 24 bytes, 8-aligned");
static_assert(offsetof(OrClosureMul, id) == 0 && offsetof(OrClosureMul, weight) == 4 && offsetof(OrClosureMul, closure) == 16, "OrClosureMul: id 0, weight 4, closure 16");
static_assert(sizeof(OrClosureAdd) == 24 && alignof(OrClosureAdd) == 8, "OrClosureAdd is 24 bytes, 8-aligned");
static_assert(offsetof(OrClosureAdd, id) == 0 && offsetof(OrClosureAdd, closureA) == 8 && offsetof(OrClosureAdd, closureB) == 16, "OrClosureAdd: id 0, closureA 8, closureB 16");
#endif
static_assert(OR_CLOSURE_MUL == -1 && OR_CLOSURE_ADD == -2, "the reserved node ids");
static_assert(OR_CLOSURE_ABI_VERSION == OR_ABI_VERSION(1, 0, 0), "the closure ABI starts at 1.0.0");

// what the strict C17 translation unit measured (layout_c17.c)
extern "C" {
size_t cl_c17_sizeof_component(void);
size_t cl_c17_alignof_component(void);
size_t cl_c17_offsetof_component_w(void);
size_t cl_c17_sizeof_mul(void);
size_t cl_c17_offsetof_mul_closure(void);
size_t cl_c17_sizeof_add(void);
size_t cl_c17_offsetof_add_closureB(void);
uint32_t cl_c17_abi_version(void);
int cl_c17_mul_id(void);
int cl_c17_add_id(void);
}

int main() {
    // run-time checks of the same table
    CL_CHECK_INT(sizeof(OrClosureComponent), 16);
    CL_CHECK_INT(alignof(OrClosureComponent), 16);
    CL_CHECK_INT(offsetof(OrClosureComponent, id), 0);
    CL_CHECK_INT(offsetof(OrClosureComponent, w), 4);
    CL_CHECK_INT(sizeof(OrClosureMul), 24);
    CL_CHECK_INT(alignof(OrClosureMul), 8);
    CL_CHECK_INT(offsetof(OrClosureMul, weight), 4);
    CL_CHECK_INT(offsetof(OrClosureMul, closure), 16);
    CL_CHECK_INT(sizeof(OrClosureAdd), 24);
    CL_CHECK_INT(alignof(OrClosureAdd), 8);
    CL_CHECK_INT(offsetof(OrClosureAdd, closureA), 8);
    CL_CHECK_INT(offsetof(OrClosureAdd, closureB), 16);
    CL_CHECK_INT(sizeof(OrClosureNode), 4);

    // The compatibility figure (E13): the parameter block starts directly after the component struct, at offset 16
    OrClosureComponent probe[2] = {};
    const char *blockOfFirst = (const char *)(probe + 1); // what `(const char *)(c + 1)` means for a component
    CL_CHECK_INT(blockOfFirst - (const char *)probe, 16);
    CL_CHECK_INT(sizeof(OrClosureComponent), 16); // the block offset IS sizeof(OrClosureComponent)

    // the ABI version and the accessors of orplugin.h
    CL_CHECK_INT(OR_ABI_MAJOR(OR_CLOSURE_ABI_VERSION), 1);
    CL_CHECK_INT(OR_ABI_MINOR(OR_CLOSURE_ABI_VERSION), 0);
    CL_CHECK_INT(OR_ABI_PATCH(OR_CLOSURE_ABI_VERSION), 0);

    // the values of the enumerators
    CL_CHECK_INT(OR_CLOSURE_MUL, -1);
    CL_CHECK_INT(OR_CLOSURE_ADD, -2);
    CL_CHECK_INT(OR_CLOSURE_BSDF, 0);
    CL_CHECK_INT(OR_CLOSURE_EDF, 1);
    CL_CHECK_INT(OR_CLOSURE_VDF, 2);
    CL_CHECK_INT(OR_CLOSURE_SIGNAL, 3);
    CL_CHECK_INT(OR_SPECTRUM_MAX, 4);

    // the strict C17 translation unit sees the same layout
    CL_CHECK_INT(cl_c17_sizeof_component(), sizeof(OrClosureComponent));
    CL_CHECK_INT(cl_c17_alignof_component(), alignof(OrClosureComponent));
    CL_CHECK_INT(cl_c17_offsetof_component_w(), offsetof(OrClosureComponent, w));
    CL_CHECK_INT(cl_c17_sizeof_mul(), sizeof(OrClosureMul));
    CL_CHECK_INT(cl_c17_offsetof_mul_closure(), offsetof(OrClosureMul, closure));
    CL_CHECK_INT(cl_c17_sizeof_add(), sizeof(OrClosureAdd));
    CL_CHECK_INT(cl_c17_offsetof_add_closureB(), offsetof(OrClosureAdd, closureB));
    CL_CHECK_INT(cl_c17_abi_version(), OR_CLOSURE_ABI_VERSION);
    CL_CHECK_INT(cl_c17_mul_id(), -1);
    CL_CHECK_INT(cl_c17_add_id(), -2);

    // For spec 049's layout test, which compares these against oslclosure.h (ClosureComponent, ClosureMul, ClosureAdd): the
    // compatibility figure is the block offset; the rest is informational (OSL's component may be larger while the offset matches)
    std::printf("LAYOUT component: sizeof %zu alignof %zu offsetof(id) %zu offsetof(w) %zu block-offset %zu  <- the compatibility figure\n", sizeof(OrClosureComponent),
                alignof(OrClosureComponent), offsetof(OrClosureComponent, id), offsetof(OrClosureComponent, w), sizeof(OrClosureComponent));
    std::printf("LAYOUT mul:       sizeof %zu alignof %zu offsetof(id) %zu offsetof(weight) %zu offsetof(closure) %zu\n", sizeof(OrClosureMul), alignof(OrClosureMul),
                offsetof(OrClosureMul, id), offsetof(OrClosureMul, weight), offsetof(OrClosureMul, closure));
    std::printf("LAYOUT add:       sizeof %zu alignof %zu offsetof(id) %zu offsetof(closureA) %zu offsetof(closureB) %zu\n", sizeof(OrClosureAdd), alignof(OrClosureAdd),
                offsetof(OrClosureAdd, id), offsetof(OrClosureAdd, closureA), offsetof(OrClosureAdd, closureB));

    return CL_RESULT("Closure_Layout");
}
