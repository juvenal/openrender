/**
 * Project: openRender
 *
 * File: version_fake.c
 *
 * Description:
 *   Spec 037, PluginAbi_VersionCheck: the "fake module table". One module that exports a version
 *   symbol of every shape data-model.md section 2 distinguishes, each under its own name, so the
 *   unit test can point a plug-in family at the symbol it wants. There is deliberately no symbol for
 *   the "absent" case: the test names one that does not exist.
 *
 *     orFakeV100        1.0.0
 *     orFakeV110        1.1.0
 *     orFakeV190        1.9.0
 *     orFakeV200        2.0.0
 *     orFakeRaw0        0x00000000 (MAJOR 0: not a version at all)
 *     orFakeRawFFFF     0xFFFFFFFF (MAJOR 65535)
 *     orFakeCounted     2.0.0, and counts how often it was called (orFakeCallCount): a cached
 *                       verdict must not call it again
 *
 *   Strict C17 from orplugin.h only.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include "orplugin.h"

OR_DECLARE_ABI_VERSION_SYMBOL(orFakeV100, OR_ABI_VERSION(1, 0, 0))
OR_DECLARE_ABI_VERSION_SYMBOL(orFakeV110, OR_ABI_VERSION(1, 1, 0))
OR_DECLARE_ABI_VERSION_SYMBOL(orFakeV190, OR_ABI_VERSION(1, 9, 0))
OR_DECLARE_ABI_VERSION_SYMBOL(orFakeV200, OR_ABI_VERSION(2, 0, 0))
OR_DECLARE_ABI_VERSION_SYMBOL(orFakeRaw0, 0x00000000u)
OR_DECLARE_ABI_VERSION_SYMBOL(orFakeRawFFFF, 0xFFFFFFFFu)

static uint32_t g_calls = 0;

OR_PLUGIN_EXPORT uint32_t orFakeCounted(void);
OR_PLUGIN_EXPORT uint32_t orFakeCounted(void) {
    ++g_calls;
    return OR_ABI_VERSION(2, 0, 0);
}

OR_PLUGIN_EXPORT uint32_t orFakeCallCount(void);
OR_PLUGIN_EXPORT uint32_t orFakeCallCount(void) {
    return g_calls;
}
