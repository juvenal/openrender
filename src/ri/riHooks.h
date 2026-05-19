#pragma once

class CRendererContext;

// Set a factory function that RiBegin will use to create the renderer context.
// Pass nullptr to restore default behaviour (creates CRendererContext directly).
// Must be called before RiBegin. Not thread-safe.
void RiSetContextFactory(CRendererContext *(*factory)());
