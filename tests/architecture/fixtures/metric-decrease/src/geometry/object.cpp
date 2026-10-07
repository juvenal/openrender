// Stand-ins for the renderer core: the metric reads source text only, and the
// in-class initializers add no "CRenderer::" text of their own.
struct CRenderer {
    static inline int a = 0;
};
struct CRendererContext {
    static inline int b = 0;
};

// CRenderer::inComment must not be counted.
const char *kLabel = "CRenderer::inString";

// Counted: CRenderer::a twice, CRendererContext::b once -> 3 refs, 2 distinct.
int geometryMetric() {
    return CRenderer::a + CRenderer::a + CRendererContext::b + (kLabel[0] == 'C' ? 1 : 0);
}
