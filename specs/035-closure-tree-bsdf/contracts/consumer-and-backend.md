# Contract: the closure consumer, the backend seam and the per-frame closure object

- **Domain**: Shading. These are internal C++ interfaces, not plug-in tables. They are not versioned on their own: the consumer's record carries closure-ABI nodes, and that ABI is versioned (R14).
- **Requirements**: FR-017, FR-021 to FR-023 and FR-025; research R10, R13 and R19.

## IClosureConsumer (`src/libshader/closures/closureConsumer.h`)

```cpp
namespace openrender::shading {
    struct ClosureBatch {
        int numPoints;                                  // numRealVertices of the batch
        const OrClosureNode *const *trees;              // numPoints roots; nullptr = empty tree
        const float *P, *N, *Ng, *dPdu, *dPdv, *I;      // stride 3 floats, camera space
        const float *u, *v;                             // stride 1
        ObjectHandle object;                            // the shaded object (opaque, CShadingContext::objectHandle())
        const sampling::SampleKey *keys;                // the points' sample identities (spec 039)
        RayType rayType; int pathDepth; BatchShape shape; // from the batch's ShaderGlobals
        ClosureMode mode;                               // the effective mode (Both or Closures); Color never reaches here
        bool fromBxdf;                                  // the trees came from a Bxdf binding
    };

    class IClosureConsumer {
      public:
        virtual ~IClosureConsumer() = default;
        // Called once per batch that built trees, on the thread that shaded it, before the batch's arena is released.
        // Everything in `batch` is valid only during the call (FR-002); copy with closureTreeCopy() to keep a tree.
        virtual void consume(CShadingContext &context, const ClosureBatch &batch) = 0;
    };
}
```

**Install points**, on `CShadingContext` (`shading.h`):

- `void setClosureConsumer(IClosureConsumer *)`: per context. Spec 042's integrator installs one per frame.
- `static void setDefaultClosureConsumer(IClosureConsumer *)`: the process default, for tests. A test installs it before `RiBegin` and clears it after `RiEnd`.
- Lookup order: the context's consumer, then the process default, then none. This is the `IIrradianceSampler` pattern (spec 032).

**The ray type of a camera hit (interim rule).** `rayType` is the batch's, as the producer set it in `ShaderGlobals`: a camera hit is `Camera`, or `None` on a grid batch (reyes and zbuffer primaries are rasterised grids, which `classifyShading()` reports as `None`; only the raytrace hider's primary hits are `Camera`). **This is interim.** Spec 058 (GitHub #267) replaces it: `classifyShading()` will report `Camera` for every camera-visible batch in every camera hider. A consumer must not build on the `None` case: treat `Camera` and `None` together as "a camera hit" until then, and expect `None` to disappear. (`ClosureMode_Both_*` and `ClosureDet_HiderParity` pin the interim values; spec 058 changes them.)

**Rules**:

1. The interface names no hider, integrator, light or renderer-core type (FR-021). `ObjectHandle`, `SampleKey`, `RayType` and `BatchShape` are already Shading or Infra types (`serviceTypes.h`, `common/sampling.h`, `shaderGlobals.h`).
2. `consume` may be called concurrently from several render threads, on different contexts. A consumer that aggregates synchronises itself.
3. The consumer must not call back into the context's shading entry points (`shade`, `iterateLights`, ...) during `consume`.
4. With no consumer installed, no tree leaves the step:
   - `both` takes the `color` path;
   - `closures` follows IR-018;
   - a Bxdf follows IR-026 (closuremode.md).

## IShaderBackend additions (`shaderBackend.h`)

```cpp
// Can this instance emit a closure tree? The built-ins answer false (every shipped shader until spec 036)
virtual bool emitsClosures(const CShaderInstance &instance) const { return false; }
// Run the instance for the current batch and write one tree per real point into `builder`. With the lighting gate open,
// the instance's Ci and Oi must equal what execute() writes, byte for byte (that is what makes `both` equal `color`)
virtual void executeClosures(CShadingContext &context, CShaderInstance &instance, float **locals,
                             openrender::shading::ClosureTreeBuilder &builder) const {}
```

- They are additions with default bodies. The `rslo` and `slo` backends and `ShadingSystem_ThirdBackend`'s `CTstBackend` compile unchanged.
- `ClosureTreeBuilder` is forward-declared. The header's sentence "No member names a language, a closure, an LLVM type or a bytecode type" becomes "... a language, a closure component, an LLVM type or a bytecode type" (FR-023).
- **Test vehicle `cldbl`** (`tests/closures/`, maintainer 2026-11-23): a wrapper backend. Its shader description names a real compiled test shader and the tree to emit. `execute` and the execution inside `executeClosures` **delegate to the real `rslo` or `slo` backend's `execute`** for the wrapped instance (format by the usual precedence), so no shading code is reimplemented and the lighting gate acts on the real builtins. `emitsClosures` answers true. The tree it emits is a pure function of the point's inputs and the wrapper's parameters. A real backend never needs this: spec 036's backend emits from its own shader.
- **Capability per instance**: `plan.capable = instance.shaderDescription()->backend->emitsClosures(instance)`. A group of one layer is its instance (gotcha 41).

## ClosureFrame (`closureFrame.h`)

```cpp
namespace openrender::shading {
    class ClosureFrame {
      public:
        explicit ClosureFrame(ClosureMode sceneMode);
        ClosureMode sceneMode() const;
        // true the first time `key` is seen in this frame (thread-safe); the caller then reports
        bool firstInFrame(const std::string &key) const;
    };
}
```

On `CShadingContext`:

- `void setClosureFrame(const ClosureFrame *)` and `const ClosureFrame *closureFrame() const`.
- With none set, the context answers scene mode `Unset` and dedups through a process-wide fallback set.

The renderer core (`renderer.cpp`):

- `beginFrame()` creates `new ClosureFrame(toShading(options->closureMode))` **before** the hider-creation loop. That loop sets the frame on each context as it creates it, next to `setSamplingConfig` (`:854`).
- `endFrame()` deletes it, after the render threads are joined. The abort path (`abortFrameTransport`) deletes it too.
- This adds no services member (R10).

## CShadingState addition

- `int lightingSuppressed;`: 0 by default. It is set and cleared only by `runSurfaceStep`, and read by the four funnels of closuremode.md (the ambient one is three sites that call one shared inline check).
