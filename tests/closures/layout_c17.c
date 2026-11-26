/*
 * Spec 035, Closure_Layout: closureAbi.h compiled as strict C17 (-std=c17 -Wall -Wextra -Wpedantic -Werror, no extensions).
 * The functions report what this translation unit measured; test_layout.cpp compares them with what C++20 measured.
 */
#include <stddef.h>
#include <stdint.h>

#include "closureAbi.h"

_Static_assert(OR_CLOSURE_MUL == -1 && OR_CLOSURE_ADD == -2, "the reserved node ids");
_Static_assert(OR_CLOSURE_ABI_VERSION == OR_ABI_VERSION(1, 0, 0), "the closure ABI starts at 1.0.0");
#if defined(__LP64__) || defined(_LP64)
_Static_assert(sizeof(OrClosureComponent) == 16 && _Alignof(OrClosureComponent) == 16, "OrClosureComponent: 16 bytes, 16-aligned");
_Static_assert(sizeof(OrClosureMul) == 24 && sizeof(OrClosureAdd) == 24, "OrClosureMul and OrClosureAdd: 24 bytes");
#endif

size_t cl_c17_sizeof_component(void) { return sizeof(OrClosureComponent); }
size_t cl_c17_alignof_component(void) { return _Alignof(OrClosureComponent); }
size_t cl_c17_offsetof_component_w(void) { return offsetof(OrClosureComponent, w); }
size_t cl_c17_sizeof_mul(void) { return sizeof(OrClosureMul); }
size_t cl_c17_offsetof_mul_closure(void) { return offsetof(OrClosureMul, closure); }
size_t cl_c17_sizeof_add(void) { return sizeof(OrClosureAdd); }
size_t cl_c17_offsetof_add_closureB(void) { return offsetof(OrClosureAdd, closureB); }
uint32_t cl_c17_abi_version(void) { return OR_CLOSURE_ABI_VERSION; }
int cl_c17_mul_id(void) { return OR_CLOSURE_MUL; }
int cl_c17_add_id(void) { return OR_CLOSURE_ADD; }
