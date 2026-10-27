/**
 * Project: openRender
 *
 * File: channelRegistry.h
 *
 * Description:
 *   The AOV/channel registry (spec 025, FR-016 to FR-018). It holds the
 *   display channels a frame can output: the five built-ins (rgb, rgba, a, z,
 *   rgbaz) and every channel declared from a declaration string or from an
 *   already-declared variable. It moved here from the renderer core's static
 *   tables, with the same ownership, the same order and the same diagnostics.
 *
 *   Declaring a new channel from a string needs the renderer core's
 *   declared-variable table; the registry reaches it through the declaration
 *   hook of variableLookup.h, so it names no other domain.
 *
 *   The renderer core's session instance is created by beginChannelRegistry()
 *   (initDeclarations) and destroyed by endChannelRegistry()
 *   (shutdownDeclarations); channelRegistry() is nullptr outside a session.
 *   Tests build their own CChannelRegistry.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef CHANNELREGISTRY_H
#define CHANNELREGISTRY_H

#include "common/containers.h"
#include "common/variable.h"
#include "displayChannel.h"

///////////////////////////////////////////////////////////////////////
// Class				:	CChannelRegistry
// Description			:	Owns the display channels, each exactly once
// Comments				:
class CChannelRegistry {
    public:
        CChannelRegistry();  // registers rgb(3,0) rgba(4,0) a(1,3) z(1,4) rgbaz(5,0), in that order
        ~CChannelRegistry(); // deletes each channel once, then frees both containers

        CChannelRegistry(const CChannelRegistry &) = delete;
        CChannelRegistry &operator=(const CChannelRegistry &) = delete;

        // Declares a channel from a declaration string. Returns the existing channel when the
        // declaration matches it; nullptr (with a diagnostic) when it clashes or is ill formed.
        CDisplayChannel *declare(const char *declaration);

        // Declares a channel for an already-declared variable. An existing channel of that name is
        // returned when it holds the same variable, otherwise nullptr (no diagnostic).
        CDisplayChannel *declare(CVariable *variable);

        // The channel called name, or nullptr.
        CDisplayChannel *retrieve(const char *name) const;

        // Marks every variable-backed channel unassigned (sampleStart = -1), in declaration order.
        void resetUsage();

        // Calls fn(channel) for each channel, in declaration order.
        template <class F>
        void forEach(F &&fn) const {
            for (int i = 0; i < inOrder.numItems; i++)
                fn(inOrder.array[i]);
        }

    private:
        void add(CDisplayChannel *channel);

        CTrie<CDisplayChannel *> byName;
        CArray<CDisplayChannel *> inOrder;
};

// The session instance.
void beginChannelRegistry();         // creates it (CRenderer::initDeclarations)
void endChannelRegistry();           // destroys it (CRenderer::shutdownDeclarations)
CChannelRegistry *channelRegistry(); // nullptr outside a session

#endif
