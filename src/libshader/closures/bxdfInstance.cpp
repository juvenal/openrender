/**
 * Project: openRender
 *
 * File: bxdfInstance.cpp
 *
 * Description:
 *   CBxdfInstance and bxdfInstanceOf() (spec 035). See bxdfInstance.h.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#include "bxdfInstance.h"

namespace openrender::shading {

    CBxdfInstance::CBxdfInstance(const OrBxdfTable *t, OrBxdfData *d, const char *h) : table(t), data(d), handle(h != nullptr ? h : "") {
        attach(); // the creator's reference: ShaderGroupRef::adopt takes it over, as for a CShaderInstance
    }

    CBxdfInstance::~CBxdfInstance() {
        if (table != nullptr && table->destroy != nullptr && data != nullptr)
            table->destroy(data);
    }

    CBxdfInstance *bxdfInstanceOf(const ShaderGroupRef &ref) {
        return dynamic_cast<CBxdfInstance *>(const_cast<CShaderGroupRefTarget *>(ref.get()));
    }

} // namespace openrender::shading
