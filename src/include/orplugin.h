/**
 * Project: openRender
 *
 * File: orplugin.h
 *
 * Description:
 *   The shared plug-in convention (spec 037, FR-002 to FR-005, FR-010). Strict C17 and C++20.
 *
 *   Every plug-in family header (dsply.h, shadeop.h, dlo.h, implicit.h) includes this header and
 *   nothing else from the tree. It defines, once:
 *     - linkage and visibility: OR_EXTERN_C*, OR_VISIBLE, OR_PLUGIN_EXPORT. Every entry point a
 *       plug-in exports is declared OR_PLUGIN_EXPORT, so it stays visible when the module is
 *       compiled with hidden-by-default symbol visibility;
 *     - the version encoding: OR_ABI_VERSION(major, minor, patch) = MAJOR << 16 | MINOR << 8 | PATCH.
 *       MINOR and PATCH are 8 bits each: a value above 255 is not representable, and a change that
 *       would need one is a MAJOR change;
 *     - the version query symbol every family declares: OR_DECLARE_ABI_VERSION_SYMBOL;
 *     - status codes (OrStatus), opaque handles (OR_DECLARE_HANDLE);
 *     - the header every function table starts with (OrTableHeader) and what the host tells a
 *       registration entry point (OrHostInfo).
 *
 *   Ownership and lifetime rules, stated once for all families:
 *     1. Whoever allocates frees. The host never frees plug-in memory and the plug-in never frees
 *        host memory.
 *     2. Host data handed to a plug-in is const and valid for a stated window (display: from start
 *        until finish returns). A plug-in that needs it longer copies it.
 *     3. A plug-in's opaque instance pointer is returned to it unchanged on every later call and is
 *        never dereferenced by the host.
 *     4. Plug-in tables and strings returned by registration entry points are static and outlive
 *        every call.
 *     5. A module is unloaded only after the last call into it has returned and nothing the host
 *        keeps (objects, function pointers, tables) refers into it.
 *     6. No C++ type, exception or std:: object crosses the boundary; a plug-in must not let an
 *        exception escape an entry point.
 *
 *   Version history of the convention (the family headers carry their own):
 *
 *   | Version | Change |
 *   |---|---|
 *   | 1.0.0 | First version (spec 037): encoding, export macros, status codes, table header. |
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#ifndef ORPLUGIN_H
#define ORPLUGIN_H

#include <stdint.h>

/* ---- linkage and visibility ---- */
#ifdef __cplusplus
#define OR_EXTERN_C extern "C"
#define OR_EXTERN_C_BEGIN extern "C" {
#define OR_EXTERN_C_END }
#else
#define OR_EXTERN_C extern
#define OR_EXTERN_C_BEGIN
#define OR_EXTERN_C_END
#endif

#if defined(_WINDOWS) || defined(WIN32)
#define OR_VISIBLE __declspec(dllexport) /* unchanged Windows branch, not a support promise */
#elif defined(__GNUC__) || defined(__clang__)
#define OR_VISIBLE __attribute__((visibility("default")))
#else
#define OR_VISIBLE
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
    OR_STATUS_OK = 0,
    OR_STATUS_ERROR = 1,       /* generic failure; the call had no effect the caller relies on */
    OR_STATUS_UNSUPPORTED = 2, /* the request is valid but this plug-in cannot honour it       */
    OR_STATUS_BAD_ARGUMENT = 3,
    OR_STATUS_OUT_OF_MEMORY = 4,
    OR_STATUS_VERSION = 5,   /* version or MAJOR mismatch                                    */
    OR_STATUS_TABLE_SIZE = 6 /* table smaller than the required size                         */
} OrStatus;

/* ---- opaque handles ---- */
#define OR_DECLARE_HANDLE(Name) typedef struct Name Name

/* ---- every function table starts with this ---- */
typedef struct OrTableHeader {
        uint32_t structSize; /* sizeof the plug-in's table, as the plug-in was compiled */
        uint32_t abiVersion; /* OR_ABI_VERSION the plug-in was compiled against           */
} OrTableHeader;

/* ---- what the host tells a registration entry point ---- */
typedef struct OrHostInfo {
        uint32_t structSize;
        uint32_t abiVersion;     /* the host's version of THIS family */
        const char *hostName;    /* "openRender"                      */
        const char *hostVersion; /* e.g. "1.1.0"                      */
} OrHostInfo;

#endif
