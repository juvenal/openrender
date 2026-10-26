# Contract: Attribute slots, the last-bound record and the pattern-node table

**Owner**: Graphics State (`attributes.h`, `patternTable.h`).
**Requirements**: FR-001, FR-004, FR-010 to FR-015, FR-024; User Stories 3 and 4; the "Default
surface", "Shading restore", "Copy-on-write clones" and "Motion-blur attribute chains" edge cases.

## Slots

```cpp
enum class EMaterialBinding : unsigned char { None, Surface, Bxdf };

class CAttributes {
public:
    const ShaderGroupRef &surface() const;       // and displacement(), atmosphere(),
    const ShaderGroupRef &bxdf() const;          // interior(), exterior()
    void bindSurface(ShaderGroupRef);            // record := Surface (even when empty), recompute
    void bindBxdf(ShaderGroupRef);               // record := Bxdf (even when empty), recompute
    void bindDisplacement(ShaderGroupRef);       // recompute; record unchanged
    void bindAtmosphere(ShaderGroupRef);
    void bindInterior(ShaderGroupRef);
    void bindExterior(ShaderGroupRef);
    void setDefaultSurface(ShaderGroupRef);      // RiWorldBegin only: recompute; record unchanged
    EMaterialBinding governingMaterial() const;  // the single query (FR-011)

    void addLight(CShaderGroupRefTarget *);      // non-owning, append, no duplicates, recompute
    void removeLight(CShaderGroupRefTarget *);   // recompute
    CActiveLight *lightSources;                  // read by Shading through lightInstanceOf()

    MapRef globalMap, causticMap;                // bound lazily by the core
    CPatternTable patterns;
    // …all other fields unchanged…
};
inline CShaderGroupRefTarget *selectVolumeShader(const CAttributes *, bool isSolidFragment, bool isExterior);
```

Behaviour:
- **Binding an empty handle** releases the old object and leaves the slot empty. For
  `bindSurface`/`bindBxdf` it also sets the record. This is today's `Surface "null"` / failed-load
  behaviour, plus the record.
- **`RiWorldBegin`** calls `setDefaultSurface()` only when `!currentAttributes->surface()`, exactly
  as today. The record stays as it was, so `classic` output is byte-identical (clarification Q2).
- **The copy constructor** copies every slot and map with a retain, the record by value, and the
  pattern table by sharing. It **prepends** each light entry, so the order reverses as today
  (research R5). The motion chain (`next`) is copied recursively, and each block keeps its own
  handles.
- **`restore(other, shading = true, …)`** assigns the six slots, copies the record and the pattern
  table, prepends the lights (reversing them), recomputes the parameters, and copies the map
  *names* only, as today. The other subsets are unchanged.
- **The bxdf slot** is ignored by `usedParameters` and `findParameter` in 025 (research R4).

## Pattern-node table

```cpp
class CPatternTable {
public:
    void define(const char *handle, ShaderGroupRef node); // replace in place or append
    const ShaderGroupRef *lookup(const char *handle) const;  // nullptr if not found; no diagnostic
    template <class F> void forEach(F &&) const;             // definition order
    int size() const;
};
```

Behaviour:
- **Visibility (US4.1)**: a definition is visible in the block that made it and in every block
  cloned from it later, which covers nested `AttributeBegin` scopes and copy-on-write clones.
- **Scope end (US4.2)**: after `AttributeEnd` the outer block is current again with its own,
  untouched table. A handle defined only inside is not found, and an outer definition of the same
  handle is found again.
- **Not found (US4.3)**: an undefined handle gives `nullptr`, and State writes nothing.
- **Lifetime (US4.4)**: a node bound into a slot stays alive through the slot's reference after
  its defining scope ends.
- **Cost**: cloning a block shares the table (O(1)). Only `define` on shared storage copies it.

## Not in 025 (FR-012)

- No `Pattern` or `Bxdf` RIB statement.
- No `reference` parameter syntax and no group assembly.
- No grammar change and no D9 name resolution.

Specs 034 and 035 consume this API.
