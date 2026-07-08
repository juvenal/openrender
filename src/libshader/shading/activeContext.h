// activeContext.h — thread-local active shading context for the JIT runtime
//
// JIT-compiled .slo shaders call back into the shading engine via rslBuiltins.cpp.
// Those C-linkage functions need the current CShadingContext without depending on
// CRenderer::activeContext (which lives in src/ri/).  This module provides the
// libshader-owned pointer that CShadingContext::execute() sets before dispatching
// to the JIT entry point and clears on return.
//
#pragma once

class CShadingContext;

namespace libshader {

extern thread_local CShadingContext *g_activeCtx;

inline void setActiveContext(CShadingContext *ctx) { g_activeCtx = ctx; }
inline CShadingContext *activeContext() { return g_activeCtx; }

} // namespace libshader
