/*
 * Spec 035, Phase 4 (T081), Bxdf_Layout (labels closures, bxdf): OrBxdfTable, OrBxdfParam and OrBxdfPoint (contracts/bxdf.md), pinned on LP64 at
 * compile time and again at run time, the header's version, and bxdfAbi.h compiled as strict C17 (bxdf_layout_c17.c) reporting the same numbers.
 *
 * Red on the starting tree: does not compile (bxdfAbi.h does not exist).
 */
#include <cstddef>
#include <cstdint>
#include <cstdio>

#include "bxdfAbi.h"
#include "closures/test_support.h"

#if defined(__LP64__) || defined(_LP64)
static_assert(sizeof(OrBxdfParam) == 48 && offsetof(OrBxdfParam, defaultValue) == 12 && offsetof(OrBxdfParam, onOutOfRange) == 32 && offsetof(OrBxdfParam, doc) == 40, "OrBxdfParam layout");
static_assert(sizeof(OrBxdfPoint) == 108 && offsetof(OrBxdfPoint, Cs) == 84 && offsetof(OrBxdfPoint, Os) == 96, "OrBxdfPoint layout");
static_assert(sizeof(OrBxdfTable) == 56 && offsetof(OrBxdfTable, name) == 8 && offsetof(OrBxdfTable, build) == 48, "OrBxdfTable layout");
#endif
static_assert(OR_BXDF_ABI_VERSION == OR_ABI_VERSION(1, 0, 0), "the Bxdf ABI starts at 1.0.0");

extern "C" {
size_t bx_c17_sizeof_param(void);
size_t bx_c17_offsetof_param_defaultValue(void);
size_t bx_c17_offsetof_param_onOutOfRange(void);
size_t bx_c17_offsetof_param_doc(void);
size_t bx_c17_sizeof_point(void);
size_t bx_c17_offsetof_point_Cs(void);
size_t bx_c17_offsetof_point_Os(void);
size_t bx_c17_sizeof_table(void);
size_t bx_c17_offsetof_table_name(void);
size_t bx_c17_offsetof_table_build(void);
uint32_t bx_c17_abi_version(void);
}

int main() {
    CL_CHECK_INT(sizeof(OrBxdfParam), bx_c17_sizeof_param());
    CL_CHECK_INT(offsetof(OrBxdfParam, defaultValue), bx_c17_offsetof_param_defaultValue());
    CL_CHECK_INT(offsetof(OrBxdfParam, onOutOfRange), bx_c17_offsetof_param_onOutOfRange());
    CL_CHECK_INT(offsetof(OrBxdfParam, doc), bx_c17_offsetof_param_doc());
    CL_CHECK_INT(sizeof(OrBxdfPoint), bx_c17_sizeof_point());
    CL_CHECK_INT(offsetof(OrBxdfPoint, Cs), bx_c17_offsetof_point_Cs());
    CL_CHECK_INT(offsetof(OrBxdfPoint, Os), bx_c17_offsetof_point_Os());
    CL_CHECK_INT(sizeof(OrBxdfTable), bx_c17_sizeof_table());
    CL_CHECK_INT(offsetof(OrBxdfTable, name), bx_c17_offsetof_table_name());
    CL_CHECK_INT(offsetof(OrBxdfTable, build), bx_c17_offsetof_table_build());
    CL_CHECK_INT(OR_BXDF_ABI_VERSION, bx_c17_abi_version());
    std::printf("OrBxdfParam %zu, OrBxdfPoint %zu, OrBxdfTable %zu bytes\n", sizeof(OrBxdfParam), sizeof(OrBxdfPoint), sizeof(OrBxdfTable));
    return CL_RESULT("Bxdf_Layout");
}
