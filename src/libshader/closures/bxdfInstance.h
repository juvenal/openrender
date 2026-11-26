/**
 * Project: openRender
 *
 * File: bxdfInstance.h
 *
 * Description:
 *   The Bxdf a `Bxdf` statement binds (spec 035, contracts/bxdf.md "Instance"): a table, the data its `create` made, the statement's handle,
 *   and the substitute surface of the interim rule IR-026 (A'). It lives in the attributes' bxdf slot as a CShaderGroupRefTarget and is
 *   never a CShaderInstance, so nothing may read the bxdf slot through shaderInstanceOf(): that is a static_cast and would turn this object
 *   into a shader. Use bxdfInstanceOf(), a checked cast that answers nullptr for anything else (a State double, an RSL instance an old
 *   test binds): a foreign object in the slot never selects the Bxdf path.
 *
 *   A Bxdf takes no instance key and no shader ordinal: it draws nothing (FR-016).
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef BXDFINSTANCE_H
#define BXDFINSTANCE_H

#include <string>

#include "bxdfAbi.h"
#include "ri/state/stateHandles.h"

namespace openrender::shading {

    class CBxdfInstance final : public CShaderGroupRefTarget {
        public:
            // Takes the table's data (`create`'s result); `destroy` runs when the last reference goes
            CBxdfInstance(const OrBxdfTable *table, OrBxdfData *data, const char *handle);
            ~CBxdfInstance() override;
            CBxdfInstance(const CBxdfInstance &) = delete;
            CBxdfInstance &operator=(const CBxdfInstance &) = delete;

            const OrBxdfTable *table;
            OrBxdfData *data;
            std::string handle;
            // A' (Q-R1, RISpec 3.2 section 4.2.4): the defaultsurface RiWorldBegin bound (set at bind, or lazily by RiWorldBegin for a Bxdf bound
            // earlier), or empty for the built-in no-shader path. Never a user-bound Surface.
            ShaderGroupRef substituteSurface;
    };

    // Checked: nullptr for an empty handle and for any object that is not a CBxdfInstance. Never use shaderInstanceOf(attributes.bxdf()).
    CBxdfInstance *bxdfInstanceOf(const ShaderGroupRef &ref);

} // namespace openrender::shading

#endif
