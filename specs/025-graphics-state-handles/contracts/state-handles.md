# Contract: State handles and their resolution

**Owner**: Graphics State (`src/ri/state/stateHandles.h`, header-only).
**Resolvers**: Shading (`src/libshader/shading/shaderRefs.h`), Hiders (`src/ri/hiders/photonMap.h`).
**Requirements**: FR-001 to FR-004, US1.3, the "Hot paths" and "Unchecked light cast" edge cases.

## State side

```cpp
class CShaderGroupRefTarget : public CRefCounter {   // anything a shader slot can hold
protected:
    CShaderGroupRefTarget() = default;
};
class CMapRefTarget : public CRefCounter {           // anything a map slot can hold
protected:
    CMapRefTarget() = default;
};

template <class T> class TStateRef {
public:
    TStateRef() noexcept = default;                  // empty
    static TStateRef adopt(T *object) noexcept;      // takes over the caller's reference
    static TStateRef retain(T *object) noexcept;     // attach()es a non-null object
    TStateRef(const TStateRef &) noexcept;           // attach()
    TStateRef(TStateRef &&) noexcept;                // no count change
    TStateRef &operator=(const TStateRef &) noexcept;// attach new, then detach old
    TStateRef &operator=(TStateRef &&) noexcept;
    ~TStateRef();                                    // detach()
    void reset() noexcept;
    T *get() const noexcept;
    explicit operator bool() const noexcept;
    friend bool operator==(const TStateRef &, const TStateRef &) noexcept;
    friend bool operator==(const TStateRef &, const T *) noexcept;
};
using ShaderGroupRef = TStateRef<CShaderGroupRefTarget>;
using MapRef         = TStateRef<CMapRefTarget>;
```

Rules:
- State never inspects a target and never casts it.
- Every operation is inline, so there is no out-of-line State symbol for Shading or Hiders to
  import.
- A null `T*` passed to `adopt`/`retain` gives an empty handle.
- `adopt` is used where today's code stores a fresh reference without attaching it: the shader
  slots and the imager. `retain` is used where today's code attaches: the photon maps. See
  data-model.md §"Ownership mapping".

## Domain-side resolution (inline, owned by the resolving domain)

```cpp
// Shading: shaderRefs.h
inline CShaderInstance *shaderInstanceOf(const CShaderGroupRefTarget *t)
    { return static_cast<CShaderInstance *>(const_cast<CShaderGroupRefTarget *>(t)); }
inline CShaderInstance *shaderInstanceOf(const ShaderGroupRef &r) { return shaderInstanceOf(r.get()); }
// DB-013: unchecked, as before spec 025. Built-in sphere and quad lights are not programmable
// instances. The consumers read only CShaderInstance members.
inline CProgrammableShaderInstance *lightInstanceOf(const CActiveLight &e)
    { return static_cast<CProgrammableShaderInstance *>(shaderInstanceOf(e.light)); }

// Hiders: photonMap.h
inline CPhotonMap *photonMapOf(const MapRef &r) { return static_cast<CPhotonMap *>(r.get()); }
```

Notes on these accessors:
- `static_cast` is the only conversion allowed. No `reinterpret_cast`, `void*`, `dynamic_cast` or
  assert.
- `CPhotonMap`'s base offset is applied by the compiler.
- For `CShaderInstance` and its derived classes the pointer value is the same as before 025,
  because every class in the chain uses single inheritance.
- The renderer core uses these same accessors. Graphics State, Geometry, Texturing and Scene
  Interface never include `shaderRefs.h` or `photonMap.h`.
- Geometry's presence checks (`surface.cpp:605`, and `geometryDispatch.cpp:281` in the core) become
  `if (attributes->displacement() && …)`. That reads State only.

## Allowed-dependency check

| Edge | Direction | Allowed |
|---|---|---|
| `shader.h` → `stateHandles.h` (`CShaderInstance` base) | Shading → State | yes |
| `photonMap.h` → `stateHandles.h` | Hiders → State | yes |
| core → `shaderRefs.h`, `photonMap.h` | Core → domain | yes |
| State → any of the above | — | **absent** (SC-001, SC-003) |
