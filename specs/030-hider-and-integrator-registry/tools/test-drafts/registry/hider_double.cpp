/**
 * Project: openRender
 *
 * File: hider_double.cpp
 *
 * Description:
 *   Spec 030 (hider and integrator registry): the test double hiders (see hider_double.h). This file is
 *   linked into the test executables that need a hider from outside the core; nothing in src/ names it.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#include "hider_double.h"

#include "hider.h"
#include "hiderAbi.hpp"
#include "hiderPeers.h"
#include "renderer.h"
#include "transport/capability.h"

SHiderDoubleCounts &hiderDoubleCounts() {
    static SHiderDoubleCounts counts;
    return counts;
}

namespace {

    // A hider that takes the core's bucket jobs and renders nothing: enough to prove that the core built it
    // once per thread, dispatched buckets to it as declared and destroyed it through the table.
    class CDoubleHider : public CHider {
        public:
            explicit CDoubleHider(int thread) : CHider(thread) {}
            ~CDoubleHider() override { ++hiderDoubleCounts().destroyed; }

            void renderingLoop() override {
                CRenderer::CJob job;
                while (true) {
                    CRenderer::dispatchJob(context.thread, job);
                    if (job.type == CRenderer::CJob::TERMINATE)
                        break;
                    if (job.type == CRenderer::CJob::BUCKET) {
                        ++hiderDoubleCounts().buckets;
                        // the bucket bookkeeping of the real hiders: the next call asks for the following bucket
                        context.currentXBucket++;
                        if (context.currentXBucket == CRenderer::xBuckets) {
                            context.currentXBucket = 0;
                            context.currentYBucket++;
                        }
                    }
                }
            }
    };

    const OrCapability kProvides[] = {{OR_CAP_DISPLAY_OUTPUT, OR_ABI_VERSION(1, 0, 0)}};

    const OrHiderAlias kAliases[] = {
        {HIDER_DOUBLE_ALIAS, nullptr, nullptr},
        {HIDER_DOUBLE_DEPRECATED_ALIAS, HIDER_DOUBLE_DEPRECATION_WARNING, nullptr},
    };

    OrStatus doublePreDisplaySetup(void) {
        ++hiderDoubleCounts().preDisplay;
        return OR_STATUS_OK;
    }

    OrHiderInstance *doubleCreate(const OrHiderCreateInfo *info) {
        SHiderDoubleCounts &c = hiderDoubleCounts();
        ++c.created;
        c.threadMask |= 1 << info->thread;
        c.numThreadsSeen = info->numThreads;
        const SHiderFrame &frame = frameOf(info->frame);
        if (frame.peers != nullptr && frame.peers->count == info->numThreads)
            ++c.peersSeen;
        return instanceOf(new CDoubleHider(info->thread));
    }

    void doubleDestroy(OrHiderInstance *instance) {
        delete hiderOf(instance); // the destructor counts
    }

    OrHiderInstance *nullCreate(const OrHiderCreateInfo *) {
        ++hiderDoubleCounts().nullCreates;
        return nullptr;
    }

    const OrHiderTable *doubleTable() {
        static const OrHiderTable table = {
            {sizeof(OrHiderTable), OR_HIDER_ABI_VERSION}, HIDER_DOUBLE_NAME, kAliases, 2, kProvides, 1, OR_HIDER_DISPATCH_BUCKETS,
            doublePreDisplaySetup, doubleCreate, doubleDestroy};
        return &table;
    }

    const OrHiderTable *nullCreateTable() {
        static const OrHiderTable table = {
            {sizeof(OrHiderTable), OR_HIDER_ABI_VERSION}, HIDER_DOUBLE_NULLCREATE_NAME, nullptr, 0, kProvides, 1, OR_HIDER_DISPATCH_BUCKETS,
            nullptr, nullCreate, doubleDestroy};
        return &table;
    }

} // namespace

OR_REGISTER_HIDER(doubleTable)
OR_REGISTER_HIDER(nullCreateTable)
