/*
 * Spec 037, PluginAbi_InstallLayouts: the headers of an installed openRender, reached through
 * find_package(openRender) alone. dsply.h includes orplugin.h; both are plain C17.
 */
#include "dsply.h"
#include "orplugin.h"

int or_consumer_version(void) {
    return (int)OR_DISPLAY_ABI_VERSION_MAJOR + (int)OR_ABI_MAJOR(OR_DISPLAY_ABI_VERSION);
}
