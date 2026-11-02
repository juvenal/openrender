# Contract: the active RI context (FR-009-FR-011, US3)

Research R7, measured facts M6.

## Interface (`src/ri/parse/riInterface.h`, Scene Interface)

```cpp
class CRiInterface {
    public:
        CRiInterface();             // assigns nothing global, attaches nothing
        virtual ~CRiInterface();    // if installed: riUninstallContext(this), after the derived destructor ran
        ...
    private:
        CRiInterface *riPrevious = nullptr;  // the context active when this one was installed
        bool riInstalled = false;
        friend void riInstallContext(CRiInterface *);
        friend void riUninstallContext(CRiInterface *);
        friend CRiInterface *riPreviousContext(const CRiInterface *);
};

// Make context the active one (renderMan), remembering the context active before it, and route
// diagnostics to it (riDiagnosticsAttach, idempotent). Installing an installed context is a bug
// (CODE_BUG, ignored).
void riInstallContext(CRiInterface *context);

// If context is the active one: the remembered context becomes active again, and diagnostics are
// re-attached (riDiagnosticsAttach) when it is non-null, detached (riDiagnosticsDetach) when it is
// null. If context is installed but not active (RiContext switched away): only the mark is cleared.
// Not installed: nothing.
void riUninstallContext(CRiInterface *context);

// The context that was active when context was installed (nullptr when none, or not installed).
CRiInterface *riPreviousContext(const CRiInterface *context);
```

`riInterface.h` is not an installed header (`CMake/openRenderPublicHeaders.cmake`), so the two
private members change no published layout.

## Call sites

| Site (today) | After |
|---|---|
| `riInterface.cpp:37,39` constructor assigns and attaches | removed |
| `riInterface.cpp:56-59` destructor clears when active | `if (riInstalled) riUninstallContext(this);` |
| `ri.cpp:311,313,322` `renderMan = factory(...)` (three `RiBegin` branches) | `riInstallContext(factory(...))` |
| `ri.cpp:459-460` `RiEnd`: `delete renderMan; renderMan = NULL;` | unchanged order: the delete uninstalls (previous is `nullptr`, so diagnostics detach after the derived destructor reported its statistics); the `NULL` assignment stays as a guard |
| `ri.cpp:287-289` `RiContext` | unchanged (activates a handle; attaches when non-null) |
| `rendererContext.cpp:6225-6226` `RiArchiveBeginV` | `riInstallContext(new CRibOut(fileName));` |
| `rendererContext.cpp:6231-6233` `RiArchiveEnd` | empty body (nothing to clear) |
| `ri.cpp:2139-2145` `RiArchiveEnd` at nesting 0 | `if (riPreviousContext(renderMan) != nullptr) delete renderMan;` (the delete restores the renderer and re-attaches) |
| `ri.cpp:110` `savedRenderMan`, `rendererContext.cpp:100-101` `extern`s | deleted |
| `previewContext.cpp:200-204` | the back-end session (backend-registration.md) |
| `dataLoad.cpp:141-142,175` | `sink = new CRiInterface(); riInstallContext(sink);` and `delete sink;` |
| tests: `blobbyTestUtils.h:53`, `test_texture_system_stream_cursor.cpp:111,229`, `test_texture_system_null_resolver.cpp:115,165`, `test_data_keys.cpp:51,57` | `riInstallContext` / `delete` (TEST-APPROVAL) |

## Rules checked by tests

1. Construct, do not install: `renderMan` unchanged; an `error()` reaches the sink installed before
   the construction.
2. Install: `renderMan == ctx`; an `error()` reaches `ctx`'s handler.
3. Uninstall: both restored to the values before the install.
4. Delete an installed, active context: as 3. Delete an uninstalled one: neither changes.
5. Nest (renderer, then archive writer): `RiArchiveEnd` makes the renderer active again with
   diagnostics re-attached; the writer's output file is complete.
6. Attach twice / detach with none: harmless (spec 023 behaviour, unchanged).
7. Mechanical: no source under `src/` or `tests/` outside `src/ri/parse/` assigns `renderMan` (regex
   `\brenderMan\s*=[^=]`; the check's own script excluded), and none names `savedRenderMan`; red on
   the starting tree (M6).

## Known difference (Q5)

Reports raised while a `CRendererContext` constructs (`beginRenderer()`: file and network setup)
reach the sink installed before `RiBegin` instead of the new context's `RiErrorPrint`. Both print
through `diagnosticsPrintDefault` (same text, stream, severity); `RiLastError` is not set for them.
