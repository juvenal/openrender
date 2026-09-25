# Phase 1 Data Model: Runtime Texture Tile-Fetch Abstraction

This feature has no persistent data store — its "data model" is the shape
of the in-memory objects involved in fetching one tile's pixel data from
an already-baked texture file, and how ownership shifts as a result of
this refactor.

## Tile Level Info

Describes one mip level's geometry — width, height, per-tile dimensions,
channel count, and sample format. Already implicitly known by every
existing `CTiledTexture`/`CBasicTexture` layer today (passed to their
constructors as separate parameters, derived from direct TIFF metadata
queries in `readMadeTexture()`/`readTexture()`, unchanged by this spec —
see `research.md` §1/§4); this spec formalizes it as a named struct
(`CTileLevelInfo`) so a `CTileSource` implementation can report it
uniformly.

| Field | Type | Description |
|---|---|---|
| `width`, `height` | int | This level's pixel dimensions |
| `tileWidth`, `tileHeight` | int | Tile dimensions (equal to `width`/`height` for the flat/un-made, non-tiled case) |
| `numChannels` | int | Samples per pixel |
| `bitsPerSample` | int | 8, 16, or 32 — matches the existing bit-depth tiers `textureLoadBlock` already branches on |
| `isFloatFormat` | bool | Whether samples are IEEE float |

## Tile Source (the new abstraction)

Represents "a place one mip level's tile pixel data can be fetched from."
One instance per level (see `research.md` §1 for why, not per texture).

**Lifecycle**: Constructed once, when a `CTiledTexture`/`CBasicTexture`
layer is constructed (inside `readMadeTexture()`/`readTexture()`);
`fetchTile()` may be called any number of times over the instance's
lifetime (once per cache-miss fault-in for that level's tiles — the
surrounding `CTextureBlock` cache means most lookups never reach
`fetchTile()` at all); destroyed when the owning layer is destroyed.

**Ownership**: Exactly one owner — the `CTextureLayer`-derived object
(`CTiledTexture<T>`/`CBasicTexture<T>`) that was constructed with it. This
replaces that object's current ownership of a `strdup`'d filename string
and a TIFF directory index (both removed) with ownership of a
`CTileSource*` instead — same one-owner, freed-in-destructor discipline
the layer already applies to its filename today, just redirected to a new
member.

**Concurrency**: A single instance's `fetchTile()` may be called
concurrently, from different threads, only when requesting *different*
tiles — the per-`CTextureBlock` lock above this seam (unchanged by this
spec) already serializes repeated concurrent requests for the *same*
tile before `fetchTile()` is ever reached for that case. Different
`CTileSource` instances (different levels, different textures) may always
be accessed fully concurrently — this is the property the new
multi-threaded test (`research.md` §6) verifies.

## TIFF Tile Source (the concrete implementation this spec adds)

A `CTileSource` backed by a baked TIFF file — one TIFF directory (IFD) per
instance, corresponding to one mip level. Owns its own copy of the
filename and directory index (replacing what `CTextureLayer` owned
before). `fetchTile()` opens the file, seeks to its directory, reads the
requested tile or (for the non-tiled case) the whole image, and closes the
file again — a per-call open/close, exactly matching
`textureLoadBlock`'s existing pattern (`"each TIFFOpen returns a fresh
handle"`), preserved rather than replaced with a cached/shared handle (see
`plan.md`'s Constraints).

| Field | Type | Description |
|---|---|---|
| `filename` | owned string | The baked texture file's path |
| `directory` | int | TIFF IFD index for this level |

No new fields beyond what `CTextureLayer` already stores today — this is
a relocation of existing data into the new object, not new state.
