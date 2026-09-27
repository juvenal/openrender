# Contract: `Option "texturecache"` RIB interface

The one new user-facing surface this spec introduces (FR-014/FR-015).
Follows this project's existing `Option` dispatch pattern exactly (see
`research.md` §7) — no new declaration mechanism, no new RIB grammar.

## RIB usage

```ribi
Option "texturecache" "enable" [1]
Option "texturecache" "directory" ["/studio/cache/textures"]
```

| Token | Type | Required | Default | Effect |
|---|---|---|---|---|
| `enable` | integer (boolean: 0 or non-zero) | No | `0` (disabled) | Turns the opt-in disk cache on for the rest of this render. |
| `directory` | string | No | unset (resolves to a system temp/cache directory at first use) | Overrides where cache entries are written/read. |

Either token may appear alone or together, in one `Option "texturecache"`
statement or split across several (matching how every other multi-token
`Option` class in this project already behaves — e.g. `"limits"`).
Setting `"directory"` without `"enable"` has no effect (the cache stays
disabled) — this is not an error, matching this project's existing
philosophy that misconfigured-but-syntactically-valid options degrade
gracefully rather than fail the render.

## Behavioral contract

- **Scope**: Render-global, like every other `Option` — takes effect for
  every subsequent unbaked-texture reference in the render, from the
  point it's declared onward. Not attribute-scoped (`Option`, unlike
  `Attribute`, is never nested/pushed-and-popped by `AttributeBegin`/
  `AttributeEnd`).
- **No effect on already-baked textures**: This option is consulted only
  inside this spec's new fallback path (reached solely on `TIFFOpen()`
  failure) — a texture reference that already opens successfully as a
  baked TIFF is completely unaffected regardless of this option's value
  (FR-002's boundary, unconditional).
- **Failure to honor `"directory"`** (unwritable location): degrades to
  in-memory-only preparation for that render, exactly as FR-007 already
  requires for any other disk-cache-write failure — never a failed or
  altered render.
- **Unknown token under this class**: reported via the same
  `error(CODE_BADTOKEN, ...)` path every other `Option` class already
  uses for an unrecognized token (matching `RiOptionV()`'s existing
  fallthrough behavior).
