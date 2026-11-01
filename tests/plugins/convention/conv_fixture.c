/**
 * Project: openRender
 *
 * File: conv_fixture.c
 *
 * Description:
 *   Spec 037, PluginAbi_Convention: one source, four plug-ins, chosen by TEST_CONV_VARIANT.
 *   Strict C17, built from orplugin.h (through conv_table.h) only.
 *
 *     conv_current   version 1.0.0, a table of exactly the required size, written as a plain C
 *                    initializer (the test compares it by memcmp with orplugin::makeTable)
 *     conv_larger    version 1.1.0, a table that also carries the MINOR 1 entry
 *     conv_smaller   version 1.0.0, a table whose structSize is below the required size, placed at the
 *                    END of a page followed by a PROT_NONE guard page: a host that reads one byte past
 *                    header.structSize touches the guard page and the test dies with a signal
 *     conv_major2    version 2.0.0, a table of the required size: the host supports MAJOR 1 only
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include <stddef.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "conv_table.h"
#include "pluginLog.h"

#define VARIANT_CURRENT 1
#define VARIANT_LARGER 2
#define VARIANT_SMALLER 3
#define VARIANT_MAJOR2 4

#if TEST_CONV_VARIANT == VARIANT_LARGER
#define FIXTURE_VERSION OR_ABI_VERSION(1, 1, 0)
#elif TEST_CONV_VARIANT == VARIANT_MAJOR2
#define FIXTURE_VERSION OR_ABI_VERSION(2, 0, 0)
#else
#define FIXTURE_VERSION OR_ABI_VERSION(1, 0, 0)
#endif

OR_DECLARE_ABI_VERSION_SYMBOL(orConvAbiVersion, FIXTURE_VERSION)

#if TEST_CONV_VARIANT == VARIANT_CURRENT

static const ConvPlugin g_table = {{sizeof(ConvPlugin), OR_ABI_VERSION(1, 0, 0)}, NULL, NULL};

#elif TEST_CONV_VARIANT == VARIANT_LARGER

static const ConvPluginV11 g_table = {{sizeof(ConvPluginV11), OR_ABI_VERSION(1, 1, 0)}, NULL, NULL, NULL};

#elif TEST_CONV_VARIANT == VARIANT_MAJOR2

static const ConvPlugin g_table = {{sizeof(ConvPlugin), OR_ABI_VERSION(2, 0, 0)}, NULL, NULL};

#elif TEST_CONV_VARIANT == VARIANT_SMALLER

/* header + the first entry only: below the MAJOR 1.0 required size (header + two entries). */
typedef struct ConvPluginShort {
        OrTableHeader header;
        void (*first)(void);
} ConvPluginShort;

static const ConvPluginShort *g_table = NULL;

/* The table at the very end of a page; the next page is PROT_NONE. */
static const ConvPluginShort *placeAtEndOfPage(void) {
    const size_t page = (size_t)sysconf(_SC_PAGESIZE);
    unsigned char *base = (unsigned char *)mmap(NULL, 2 * page, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    ConvPluginShort table;

    if (base == (unsigned char *)MAP_FAILED)
        return NULL;
    table.header.structSize = (uint32_t)sizeof(ConvPluginShort);
    table.header.abiVersion = OR_ABI_VERSION(1, 0, 0);
    table.first = NULL;
    memcpy(base + page - sizeof(ConvPluginShort), &table, sizeof(ConvPluginShort));
    mprotect(base + page, page, PROT_NONE);
    return (const ConvPluginShort *)(base + page - sizeof(ConvPluginShort));
}

#endif

OR_PLUGIN_EXPORT const void *orConvPlugin(const OrHostInfo *host);
OR_PLUGIN_EXPORT const void *orConvPlugin(const OrHostInfo *host) {
    (void)host;
    OR_TEST_LOG("orConvPlugin");
#if TEST_CONV_VARIANT == VARIANT_SMALLER
    if (g_table == NULL)
        g_table = placeAtEndOfPage();
    return g_table;
#else
    return &g_table;
#endif
}

OR_TEST_LOG_UNLOAD()
