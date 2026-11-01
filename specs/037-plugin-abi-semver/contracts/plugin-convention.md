# Contract: the shared plug-in convention (FR-002..FR-005, FR-010, US6, SC-003, SC-010)

Files (Infra: `orplugin.h` and `orplugin.hpp` in `src/include/`, installed in the layout's include directory, self-contained `<ORENDERHOME>/include` and FHS `<prefix>/include/openRender/`; `pluginAbi.{h,cpp}` in `src/common/`):

| File | Installed | Language | Content |
|---|---|---|---|
| `orplugin.h` | yes | C17 / C++20 | encoding, export macros, status codes, table header, host info, handle macro |
| `orplugin.hpp` | yes | C++20 only | convenience wrappers over `orplugin.h` |
| `pluginAbi.h` / `pluginAbi.cpp` | no | C++20 | the one host-side version check and table check (`libRIBCommon`) |

Every family header (`dsply.h`, `shadeop.h`, `dlo.h`, `implicit.h`) includes `orplugin.h` and
nothing else from the tree.

---

## 1. `orplugin.h` (public, C)

```c
#ifndef ORPLUGIN_H
#define ORPLUGIN_H
#include <stdint.h>

/* ---- linkage and visibility ---- */
#ifdef __cplusplus
#  define OR_EXTERN_C       extern "C"
#  define OR_EXTERN_C_BEGIN extern "C" {
#  define OR_EXTERN_C_END   }
#else
#  define OR_EXTERN_C       extern
#  define OR_EXTERN_C_BEGIN
#  define OR_EXTERN_C_END
#endif
#if defined(_WINDOWS) || defined(WIN32)
#  define OR_VISIBLE __declspec(dllexport)      /* unchanged Windows branch, not a support promise */
#elif defined(__GNUC__) || defined(__clang__)
#  define OR_VISIBLE __attribute__((visibility("default")))
#else
#  define OR_VISIBLE
#endif
#define OR_PLUGIN_EXPORT OR_EXTERN_C OR_VISIBLE

/* ---- version encoding: MAJOR<<16 | MINOR<<8 | PATCH ---- */
#define OR_ABI_VERSION(major, minor, patch) \
    ((uint32_t)(((uint32_t)(major) << 16) | (((uint32_t)(minor) & 0xFFu) << 8) | ((uint32_t)(patch) & 0xFFu)))
#define OR_ABI_MAJOR(v) ((uint32_t)(v) >> 16)
#define OR_ABI_MINOR(v) (((uint32_t)(v) >> 8) & 0xFFu)
#define OR_ABI_PATCH(v) ((uint32_t)(v) & 0xFFu)

/* ---- version query symbol (one per family, distinct names) ---- */
#define OR_DECLARE_ABI_VERSION_SYMBOL(symbol, version) \
    OR_PLUGIN_EXPORT uint32_t symbol(void);            \
    OR_PLUGIN_EXPORT uint32_t symbol(void) { return (version); }

/* ---- status codes ---- */
typedef enum OrStatus {
    OR_STATUS_OK            = 0,
    OR_STATUS_ERROR         = 1, /* generic failure; the call had no effect the caller relies on */
    OR_STATUS_UNSUPPORTED   = 2, /* the request is valid but this plug-in cannot honour it      */
    OR_STATUS_BAD_ARGUMENT  = 3,
    OR_STATUS_OUT_OF_MEMORY = 4,
    OR_STATUS_VERSION       = 5, /* version or MAJOR mismatch                                   */
    OR_STATUS_TABLE_SIZE    = 6  /* table smaller than the required size                        */
} OrStatus;

/* ---- opaque handles ---- */
#define OR_DECLARE_HANDLE(Name) typedef struct Name Name

/* ---- every function table starts with this ---- */
typedef struct OrTableHeader {
    uint32_t structSize; /* sizeof the plug-in's table, as the plug-in was compiled */
    uint32_t abiVersion; /* OR_ABI_VERSION the plug-in was compiled against          */
} OrTableHeader;

/* ---- what the host tells a registration entry point ---- */
typedef struct OrHostInfo {
    uint32_t    structSize;
    uint32_t    abiVersion;  /* the host's version of THIS family */
    const char *hostName;    /* "openRender"                      */
    const char *hostVersion; /* e.g. "1.1.0"                      */
} OrHostInfo;

#endif
```

Registration shape for a family built on the convention (display v2 is the first):

```c
OR_PLUGIN_EXPORT const <Type>Plugin *or<Type>Plugin(const OrHostInfo *host);
```

The returned table is static storage owned by the plug-in and must stay valid until the module is
unloaded. Returning `NULL` means "this module does not provide the family".

## 2. `orplugin.hpp` (public, C++ convenience)

```cpp
#include "orplugin.h"
#include <cstddef>
#include <type_traits>
namespace orplugin {
    enum class Status : int { Ok = OR_STATUS_OK, Error = OR_STATUS_ERROR, /* ... */ };

    template <class Table> constexpr Table makeTable(uint32_t abiVersion) noexcept {
        static_assert(std::is_standard_layout_v<Table> && std::is_trivially_copyable_v<Table>);
        static_assert(offsetof(Table, header) == 0, "a plug-in table starts with OrTableHeader");
        Table t{};                       // every entry point null until set
        t.header.structSize = sizeof(Table);
        t.header.abiVersion = abiVersion;
        return t;
    }
}
```

Tables built with `makeTable` are byte-identical to a hand-written C initializer of the same table
(US6-3; checked by a test that compares the two with `memcmp`). Table types therefore declare the
header as a member named `header` (C code reads `table->header.structSize`).

## 3. Host side (`pluginAbi.h`, internal)

```cpp
#define OR_PLUGIN_UNVERSIONED_SUNSET "1.2.0" // FR-007: the one constant the later release edits

struct SPluginFamily {
    const char *interfaceName;   // "display", "shadeop", "dlo", "implicit", "dynamicload"
    const char *versionSymbol;   // "orDisplayAbiVersion", ...
    uint32_t hostVersion;        // newest version the host implements
    const uint32_t *supported;   // per supported MAJOR, the host's version of it (e.g. {1.0.0, 2.0.0})
    int numSupported;
    bool warnWhenUnversioned;    // false only for dynamicload (FR-009)
};

enum class EPluginVerdict { Accept, AcceptNewerMinor, AcceptUnversioned, Reject };
struct SPluginVersionVerdict { EPluginVerdict verdict; uint32_t pluginVersion; bool versioned; };

// The one routine (FR-004). Resolves family.versionSymbol in module, calls it, applies
// data-model §2, prints at most one diagnostic, and caches the verdict per
// (interfaceName, realpath(path)) for the process. Thread-safe.
SPluginVersionVerdict checkPluginVersion(const SPluginFamily &family, void *module, const char *path);

// Table size rule (data-model §3). Reads 8 bytes at most before deciding; never reads past
// header.structSize. Diagnoses and returns false on rejection.
bool checkPluginTable(const SPluginFamily &family, const char *path, const OrTableHeader *table,
                      size_t requiredSize, uint32_t versionFromSymbol);
```

Diagnostic texts (exact wording fixed at tasks; each contains the listed items):

| Case | Severity / code | Contains |
|---|---|---|
| Reject | error, `CODE_VERSION` (already in `common/diagnostics.h:40`; no vocabulary change) | path, interface, plug-in `M.m.p` and `0x%08x`, host `M.m.p`, supported MAJORs, "rebuild against the installed header" |
| Newer MINOR | warning, `CODE_VERSION` | path, interface, both versions, "some features of the plug-in may be unused" |
| Unversioned | warning, `CODE_VERSION` | path, interface, "treated as 1.0.0", "will be rejected from openRender 1.2.0" |
| Table too small | error | path, interface, `structSize`, required size |

## 4. Ownership and lifetime rules (all families, stated once)

1. Whoever allocates frees. The host never frees plug-in memory and the plug-in never frees host
   memory.
2. Host data handed to a plug-in is `const` and valid for a stated window (display: from `start`
   until `finish` returns). A plug-in that needs it longer copies it.
3. A plug-in's opaque instance pointer is returned to it unchanged on every later call and is never
   dereferenced by the host.
4. Plug-in tables and strings returned by registration entry points are static and outlive every
   call.
5. A module is unloaded only after the last call into it has returned and nothing the host keeps
   (objects, function pointers, tables) refers into it.
6. No C++ type, exception or `std::` object crosses the boundary; plug-ins must not let exceptions
   escape an entry point.

## 5. Which families follow the convention, and where they cannot

| Family | Version symbol | Function table | Why not a table |
|---|---|---|---|
| display 2.x | yes | `OrDisplayPlugin` | — |
| shadeop 1.x | yes | no: `<name>_shadeops[]` + named functions | a table is a MAJOR change; legacy RenderMan shape |
| dlo 1.x | yes | no: `dloInit`/`dloIntersect`/`dloTini` | same |
| implicit 1.x | yes | no: four named functions | same |
| dynamicload | optional | no: RenderMan Interface's fixed names | defined by the RenderMan Interface |

## 6. Tests

- `PluginAbi_HeadersC17` / `PluginAbi_HeadersCxx20`: one TU per header alone and one with all
  five, as `-std=c17 -Wall -Wextra -Wpedantic -Werror` and C++20 with the project warnings
  (SC-003, US2-1/2).
- `PluginAbi_VersionCheck`: unit test of `checkPluginVersion` over a fake module table (every row
  of data-model §2, raw-garbage value, cache replay prints nothing).
- `PluginAbi_Convention`: the `conv_*` plug-ins: current and larger accepted, smaller rejected
  without touching the guard page, MAJOR 2 rejected; C++ `makeTable` equals the C initializer
  (SC-010, US6).
- `PluginAbi_VersionHistory`: mechanical: every header's version-history table parses, no MINOR/PATCH above 255, the table's newest row equals the header macros (FR-047).
- `PluginAbi_SingleCheckRoutine`: `nm` over the built libraries: `checkPluginVersion` defined only
  in `libRIBCommon`, referenced by the display host, the shadeop library and the procedures objects
  (SC-008).
