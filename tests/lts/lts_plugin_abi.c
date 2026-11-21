/* LTS check: the public plug-in headers agree with themselves.
 *
 * Each family's packed OR_ABI_VERSION must equal the MAJOR/MINOR/PATCH macros
 * published next to it. A release branch that bumps one and not the other
 * fails this test. The numeric versions are not frozen here: an intentional
 * bump that updates every macro in the header still passes.
 */
#include "dlo.h"
#include "dsply.h"
#include "implicit.h"
#include "orplugin.h"
#include "ptcapi.h"
#include "ri.h"
#include "rslo.h"
#include "shadeop.h"

#include <stdio.h>
#include <stdint.h>

static int check_family(const char *name, uint32_t packed, uint32_t major, uint32_t minor, uint32_t patch) {
    if (OR_ABI_MAJOR(packed) != major || OR_ABI_MINOR(packed) != minor || OR_ABI_PATCH(packed) != patch ||
        packed != OR_ABI_VERSION(major, minor, patch)) {
        fprintf(stderr, "%s: packed ABI %u does not match %u.%u.%u\n", name, packed, major, minor, patch);
        return 1;
    }
    printf("%s ABI %u.%u.%u\n", name, major, minor, patch);
    return 0;
}

int main(void) {
    int failed = 0;

    failed |= check_family("display", OR_DISPLAY_ABI_VERSION, OR_DISPLAY_ABI_VERSION_MAJOR,
                            OR_DISPLAY_ABI_VERSION_MINOR, OR_DISPLAY_ABI_VERSION_PATCH);
    failed |= check_family("shadeop", SHADEOP_ABI_VERSION, SHADEOP_ABI_VERSION_MAJOR, SHADEOP_ABI_VERSION_MINOR,
                            SHADEOP_ABI_VERSION_PATCH);
    failed |= check_family("dlo", DLO_ABI_VERSION, DLO_ABI_VERSION_MAJOR, DLO_ABI_VERSION_MINOR, DLO_ABI_VERSION_PATCH);
    failed |= check_family("implicit", IMPLICIT_ABI_VERSION, IMPLICIT_ABI_VERSION_MAJOR, IMPLICIT_ABI_VERSION_MINOR,
                            IMPLICIT_ABI_VERSION_PATCH);

    /* DynamicLoad is declared beside dlo and has no separate MAJOR macros.
     * Require a non-zero packed version that round-trips through the accessors. */
    if (OR_DYNAMICLOAD_ABI_VERSION == 0 ||
        OR_DYNAMICLOAD_ABI_VERSION != OR_ABI_VERSION(OR_ABI_MAJOR(OR_DYNAMICLOAD_ABI_VERSION),
                                                     OR_ABI_MINOR(OR_DYNAMICLOAD_ABI_VERSION),
                                                     OR_ABI_PATCH(OR_DYNAMICLOAD_ABI_VERSION))) {
        fprintf(stderr, "dynamicload: ABI version %u does not round-trip\n", OR_DYNAMICLOAD_ABI_VERSION);
        failed = 1;
    } else {
        printf("dynamicload ABI %u.%u.%u\n", OR_ABI_MAJOR(OR_DYNAMICLOAD_ABI_VERSION),
               OR_ABI_MINOR(OR_DYNAMICLOAD_ABI_VERSION), OR_ABI_PATCH(OR_DYNAMICLOAD_ABI_VERSION));
    }

    /* Touch a symbol from each installed C API so a removed header fails the link of this file. */
    (void)(RiBegin);
    (void)(RiEnd);
    (void)(PtcCreatePointCloudFile);
    (void)(rsloGet);

    return failed ? 1 : 0;
}
