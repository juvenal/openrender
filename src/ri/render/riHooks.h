#pragma once

#include <cstdio>

// ri.cpp/RiBegin() must never name CRendererContext or CRibOut directly: doing
// so would force any consumer that links this file (e.g. a future geometry-only
// "vector" library) to also link the full renderer and/or the RIB-output code,
// and everything they pull in with them (shading, LLVM). Both concrete
// implementations register themselves through the hooks below instead --
// CRendererContext's default factory in rendererContext.cpp, CRibOut's in
// ribOut.cpp -- each via a static initializer, so simply linking those
// translation units is what makes RiBegin's corresponding mode available.
class CRiInterface;

// Overrides what RiBegin() constructs for its "full render" mode (both the
// net-string and no-argument code paths). Existing behaviour, unchanged:
// when a factory is registered, RiBegin ignores the parsed rib-file/net-string
// (capture-only test contexts don't need real file/net parsing), so the
// factory itself takes no arguments. Pass nullptr to remove the override and
// fall back to whichever factory rendererContext.cpp registered by default.
// Must be called before RiBegin. Not thread-safe.
void RiSetContextFactory(CRiInterface *(*factory)());

// --- Registration, not override: called once each by a static initializer in
// the implementation's own translation unit. Not part of the public test hook
// API above; there is currently no need to override either default. ---

// Registers the default "full render" constructor. ribFile/netString are the
// two strings RiBegin() parses out of a "#...rib:...net:..." name; both null
// means the plain no-argument construction (the common case).
void RiRegisterDefaultContextFactory(CRiInterface *(*factory)(const char *ribFile, const char *netString));

// Registers the "write RIB back out" constructor. Exactly one of name/stream
// is set: name for RiBegin(<plain filename>), stream for RiBegin(NULL) writing
// to stdout (or any other pre-opened destination a caller supplies).
void RiRegisterRibOutFactory(CRiInterface *(*factory)(const char *name, FILE *stream));
