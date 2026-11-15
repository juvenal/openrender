# Contract: GI files and the irradiance wire record (FR-014, FR-015, FR-022)

## `.gpm` photon map file: layout unchanged

```
ropen header      magic, version[4], strlen("PhotonMap"), "PhotonMap\0"        (34 bytes)
CMap::write       int numItems, int maxItems (== numItems after balance), CPhoton[numItems + 1], float bmin[3], float bmax[3]
CPhotonStore      float fromWorld[16], float toWorld[16], float maxPower (the square root of the largest |C|^2)
```

What changes is only the content: every byte defined (the two padding pairs of each item zero, item 0 zero), the items in the
balanced order of the canonical photon order, `maxPower` computed from this frame's photons only. Files written by the starting
tree read as before (`CMap::read` is unchanged; key 0 for every photon read).

## `.icache` irradiance cache file: layout unchanged

```
int maxDepth
node:  vector centre, float side, int32 count, count x record (P, N, irradiance, coverage, envdir, gP[21], gR[21], dP: field by field), int32 hasChild[8], children depth-first
```

Content: the records of each node in canonical order (fixed records in file order, then new records by key), `dP` the committed
(clamped) radius. Read: records **appended** in file order (was: prepended, so each node's list was reversed on read, gotcha 37).
A file written by the starting tree reads in its file order, which was that tree's newest-first list order; lookups over it are
then the same on every read. No key is written (FR-022: no format change).

## Irradiance wire record (`CRemoteICacheChannel`, `-P:N`)

Framing unchanged: per group, `int count` then `count` raw `CCacheSample`s (240 bytes each); a count of 0 ends the frame.

| | Starting tree | After spec 040 |
|---|---|---|
| what a server sends | its whole tree at frame end (file records included) | its pending set only (the records it made this frame), in sorted order, one group |
| `dP` | the clamped radius | the raw radius (the commit clamps on the client) |
| bytes 232-239 (the `next` slot) | the server's pointer value (undefined on the receiver) | **decision D4**: the record's `maxError` as a 4-byte float, then 4 zero bytes |
| what the client does | `insertReceived`: places by raw `dP`, no `K` (#161), no clamp | `receivePending(record, maxError)`, then the canonical commit with the client's own `F` |

Record size stays 240 bytes (FR-022). D4's alternatives: grow `CIrradianceRecord` by a `maxError` field (record 248 bytes: a wire
size change and a `static_assert` change), or send one `maxError` per group with an extended framing (a framing change). The
recommendation is the slot, which changes the meaning of 8 bytes that carry nothing today.

## Photon pass under `-P:N`

Measured on the starting tree (research.md R3): the photon pass under `-P:2` does not complete. Spec 040 does not distribute the
photon pass; see research.md R3 for the decision (D6).
