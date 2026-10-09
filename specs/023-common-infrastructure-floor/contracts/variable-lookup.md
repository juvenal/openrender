# Contract: cross-domain interface changes (spec 023)

The Principle VIII gate requires every added or altered cross-domain dependency to have its
interface specified. This spec has two.

## 1. Graphics State variable-lookup hook (new)

**Owner**: Graphics State (`src/ri/state/variableLookup.{h,cpp}`).
**Implementer and installer**: the renderer core (Core → Graphics State is allowed).
**Consumer**: `src/ri/state/pl.cpp`, after `pl` moves from `src/ri/core/` to Graphics State.

```cpp
class CVariable;

using TVariableLookup = CVariable *(*)(const char *name);

// Installs the lookup used to resolve declared variable names (nullptr uninstalls).
void installVariableLookup(TVariableLookup lookup);

// Returns lookup(name), or nullptr when no lookup is installed.
CVariable *lookupVariable(const char *name);
```

**Behavior**
- `CRenderer::initDeclarations()` installs `&CRenderer::retrieveVariable`, and
  `CRenderer::shutdownDeclarations()` installs `nullptr`. Both are compiled into `ribRender` and
  `ribVector`, so `orender`, the preview and the `.orenderrc` bootstrap in `RiBegin` all get it.
- `pl.cpp` replaces `CRenderer::retrieveVariable(x)` with `lookupVariable(x)` at its two call sites.
  Results are identical whenever declarations are initialized, which is every path that parses a
  parameter list today.
- With nothing installed, a lookup returns `nullptr`. `pl.cpp` already handles that as "not
  declared": an inline declaration (`"uniform float foo"`) is still parsed with `parseVariable`.
- Single-threaded install, matching the declaration lifecycle. Reads are plain loads, as today's
  static call.

**Dependency edges**:
- removes Infra → Core (`pl.cpp` → `CRenderer::retrieveVariable`);
- adds only Core → Graphics State (`rendererDeclarations.cpp` → `installVariableLookup`), which is
  allowed.

## 2. `CRendererServices::globalMemory()` removed (Shading-owned interface)

**Owner**: Shading (`src/libshader/shading/rendererServices.h`).

- **Removed**: `virtual CMemPage *globalMemory() const = 0;` and its override in
  `src/ri/render/rendererServicesImpl.h`.
- **Replacement**: Shading code that used it (`shading.cpp:435`) uses `globalArena`
  (`common/memory.h`) directly. Shading → Infra is allowed.
- **Compatibility**: `CRendererServices` is internal and not installed, so there are no external
  implementers. The only implementation is the renderer core's, and it changes in the same
  commit.

## 3. Plug-in interfaces

None changed: `shadeop.h`, `dlo.h`, `implicit.h`, `dsply.h` and `ptcapi.h` are untouched, so no
Semantic Version bump (DB-003 is unaffected). `ri.h` is byte-identical.
