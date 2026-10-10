# Contract: common-layer C++ API (spec 023)

These are the headers `libRIBCommon` provides to every domain after this feature. Names are
binding; bodies are sketches. Every header lives in `src/common/` and is included as
`"common/<name>.h"`.

## 1. `common/diagnostics.h` (new; replaces `src/ri/parse/error.h`)

```cpp
enum EErrorCode { /* verbatim from parse/error.h: CODE_NOERROR ... CODE_RESOLUTION, same order and values */ };

enum EDiagnosticSeverity { DIAGNOSTIC_INFO, DIAGNOSTIC_WARNING, DIAGNOSTIC_ERROR, DIAGNOSTIC_SEVERE };

class CDiagnosticSink {
    public:
        virtual ~CDiagnosticSink() = default;
        // message is the fully formatted text, exactly as today's RiError receives it
        virtual void report(EErrorCode code, EDiagnosticSeverity severity, const char *message) = 0;
};

// Installs `sink` process-wide (nullptr = default sink) and returns the previously installed one.
// Thread-safe (atomic slot); callers restore the returned pointer when they are done.
CDiagnosticSink *installDiagnosticSink(CDiagnosticSink *sink);

// RiErrorPrint's stream rules, implemented once (used by the default sink and by RiErrorPrint):
//   SEVERE  -> stderr, fflush, exit(-1)
//   ERROR   -> stderr, fflush
//   WARNING -> stdout, fflush
//   INFO    -> stdout, fflush, only when current_log_level <= LogLevel::INFO
void diagnosticsPrintDefault(EDiagnosticSeverity severity, const char *message);

// Unchanged signatures (and mangled names): format with vsnprintf into an OS_MAX_PATH_LENGTH
// buffer, then dispatch to the installed sink or the default.
void error(EErrorCode, const char *, ...);
void warning(EErrorCode, const char *, ...);
void fatal(EErrorCode, const char *, ...);
void info(EErrorCode, const char *, ...);
```

**Guarantees**

- No diagnostic path dereferences an RI context. With no sink installed, the default rules apply
  (FR-002).
- The slot is the only process-wide diagnostic state, and its single definition is in
  `libRIBCommon` (FR-008, checked by `Common_StateOwnership`).
- Reporting is safe from any thread, and during static initialization or teardown: the slot is
  constant-initialized to `nullptr`.

**Scene Interface implementation** (stays in `src/ri/parse/`, not part of common):
- `CRiDiagnosticSink::report()` calls
  `renderMan->RiError(translate(code), toRIE(severity), message)` while `renderMan != NULL`.
  Otherwise it calls `diagnosticsPrintDefault`.
- `toRIE` maps each severity to its RI code: `INFO` → `RIE_INFO`, `WARNING` → `RIE_WARNING`,
  `ERROR` → `RIE_ERROR`, `SEVERE` → `RIE_SEVERE`.
- There is one static instance, attached **idempotently**:
  - `riDiagnosticsAttach()` is called from the `CRiInterface` constructor. When nothing is attached
    yet, it saves the pointer `installDiagnosticSink` returns, exactly once; otherwise it does
    nothing.
  - `riDiagnosticsDetach()` restores the saved pointer wherever the active context stops being
    active (refined during T021, 2026-10-10):
    - in `~CRiInterface` when `renderMan == this`, which covers `RiEnd`'s `delete renderMan`, the
      data viewer's teardown and `RiArchiveEnd`'s nested delete. It runs after the derived
      context's own destructor, so what that destructor reports still reaches its handler;
    - after the preview's explicit `renderMan = nullptr` (`previewContext.cpp`), whose context
      outlives that reset.
  - Re-attach points: `RiArchiveEnd` after `renderMan = savedRenderMan`, and `RiContext(handle)`.
    Both are idempotent.
- `RiErrorPrint` sets `RiLastError` as today, then calls `diagnosticsPrintDefault`.

## 2. `common/memory.h` (moved from `src/ri/core/memory.h`, extended)

Kept verbatim:
- `CMemPage`;
- `memoryInit`/`memoryTini`/`memoryNewPage`/`memoryDeletePage`;
- `ralloc(int, CMemPage *&)`, `rstrdup(const char *, CMemPage *&)`;
- `memBegin`/`memEnd`/`memSave`/`memRestore`, `TMemCheckpoint`.

Added:

```cpp
class CArena {
    public:
        constexpr CArena() : top(nullptr) {}
        CMemPage *top;           // current page of the stack (same role as the old CMemPage *)
        void init();             // == memoryInit(top)
        void tini();             // == memoryTini(top)
};

inline void *ralloc(int size, CArena &arena) { return ralloc(size, arena.top); }
inline char *rstrdup(const char *s, CArena &arena) { return rstrdup(s, arena.top); }

#define arenaBegin(__arena)              memBegin((__arena).top)
#define arenaEnd(__arena)                memEnd((__arena).top)
#define arenaSave(__data, __arena)       memSave(__data, (__arena).top)
#define arenaRestore(__data, __arena)    memRestore(__data, (__arena).top)

extern CArena globalArena;   // the process-wide scratch arena (replaces CRenderer::globalMemory)
```

**Guarantees**
- Allocation semantics are identical to `CRenderer::globalMemory`: page sizes, 8-byte alignment and
  stack discipline.
- `globalArena` has one definition per process (FR-008). It is constant-initialized, and lifetime
  is managed by the same four `init`/`tini` sites as today.

## 3. `common/variable.h` + `variable.y`/`variable.l` (moved from `src/ri/parse/`)

```cpp
#define VARIABLE_NAME_LENGTH 64            // moved from ri_config.h
class CVariable { /* fields unchanged */ };
int parseVariable(CVariable *, const char *name, const char *decl);   // unchanged signature
```

The grammar is generated in `@build/src/common/` with the `var` prefix. Every currently accepted
declaration produces identical fields, and every rejected one the same diagnostics (FR-005,
golden table).

## 4. Vocabulary

- `common/rendererc.h` is moved from `src/ri/render/rendererc.h` with its contents unchanged.
- `common/riVocabulary.h` (new) is an `extern "C"` header that declares:
  - the `Rt*` typedefs and RI macros, character-identical to `ri.h`;
  - every `RI_*` token as `extern RtToken RI_X;`;
  - the standard basis matrices (`RiBezierBasis`…) and filter functions (`RiGaussianFilter`…)
    already defined in `common/rslConstants.cpp`.
- `common/riTokens.cpp` (new) holds the token definitions, moved verbatim from `ri.cpp`.
- `src/ri/parse/ri.h` (public, installed) is **unchanged**. Its token declarations must match
  `riVocabulary.h` name for name; `Common_VocabularyConsistency` enforces this.

## 5. Moved verbatim (path change only)

| From | To |
|---|---|
| `src/ri/core/refCounter.{h,cpp}` | `common/refCounter.{h,cpp}` |
| `src/ri/core/fileResource.{h,cpp}` | `common/fileResource.{h,cpp}` |
| `src/ri/core/stats.{h,cpp}` | `common/stats.{h,cpp}` |
| `src/ri/core/random.{h,cpp}` | `common/random.{h,cpp}` |
| `src/ri/core/atomic.h` | `common/atomic.h` |

## 6. Log level

`src/includes/logging.hpp` changes `inline LogLevel current_log_level` to `extern LogLevel
current_log_level;`. The single definition goes in `common/logging.cpp`, with the same initial
value, `LogLevel::INFO`. Everything else in `logging.hpp` is unchanged.
