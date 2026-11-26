/*
 * Spec 035, Phase 4, Bxdf_Layout: bxdfAbi.h compiled as strict C17 (-std=c17 -Wall -Wextra -Wpedantic -Werror, no extensions).
 * The functions report what this translation unit measured; test_bxdf_layout.cpp compares them with what C++20 measured.
 */
#include <stddef.h>
#include <stdint.h>

#include "bxdfAbi.h"

_Static_assert(OR_BXDF_ABI_VERSION == OR_ABI_VERSION(1, 0, 0), "the Bxdf ABI starts at 1.0.0");
#if defined(__LP64__) || defined(_LP64)
_Static_assert(sizeof(OrBxdfParam) == 48, "OrBxdfParam: 48 bytes");
_Static_assert(sizeof(OrBxdfPoint) == 108, "OrBxdfPoint: 108 bytes");
_Static_assert(sizeof(OrBxdfTable) == 56, "OrBxdfTable: 56 bytes");
#endif

size_t bx_c17_sizeof_param(void) { return sizeof(OrBxdfParam); }
size_t bx_c17_offsetof_param_defaultValue(void) { return offsetof(OrBxdfParam, defaultValue); }
size_t bx_c17_offsetof_param_onOutOfRange(void) { return offsetof(OrBxdfParam, onOutOfRange); }
size_t bx_c17_offsetof_param_doc(void) { return offsetof(OrBxdfParam, doc); }
size_t bx_c17_sizeof_point(void) { return sizeof(OrBxdfPoint); }
size_t bx_c17_offsetof_point_Cs(void) { return offsetof(OrBxdfPoint, Cs); }
size_t bx_c17_offsetof_point_Os(void) { return offsetof(OrBxdfPoint, Os); }
size_t bx_c17_sizeof_table(void) { return sizeof(OrBxdfTable); }
size_t bx_c17_offsetof_table_name(void) { return offsetof(OrBxdfTable, name); }
size_t bx_c17_offsetof_table_build(void) { return offsetof(OrBxdfTable, build); }
uint32_t bx_c17_abi_version(void) { return OR_BXDF_ABI_VERSION; }
