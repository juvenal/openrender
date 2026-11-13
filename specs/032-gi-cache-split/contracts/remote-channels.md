# Contract: remote channels (cache and point cloud) without friendship

**Requirements**: FR-011, FR-012; US4; Q7. **Research**: R7, R8.

## Wire layout

| Message | Today | After |
|---|---|---|
| cache setup | `vector center`, `float side` of the root | unchanged (`rootCenter()`, `rootSide()`) |
| cache frame | per non-empty node, in stack order: `int n`, then `n` raw `CCacheSample` (232 bytes on LP64: fields 0-223, `next` 224-231); the terminator `int 0` | per node the same, but `CCacheSample` is 240 bytes: fields 0-223, **`key` 224-231**, `next` 232-239 |
| point-cloud setup | `char[1024]`: comma-separated channel names, then an uninitialised tail | unchanged |
| point-cloud frame | `int numItems`, `CPointCloudPoint[numItems]` raw, `float[dataSize * numItems]` raw | unchanged |

Masked when compared (T-A):
- every `next` field;
- the bytes of the 1024-byte setup buffer after the first NUL;
- the 2 padding bytes after `flags` in each raw `CPointCloudPoint`: the third mask, approved with OQ-8 on
  2026-11-07. They are never assigned, so on base they may be nonzero.

`CCacheSample` has no padding (all floats, then the pointer at an 8-aligned offset). The after-state comparison
removes the 8 key bytes at offset 224 of each record and then compares with the base recording; nothing else may
differ. Compatibility is same-build only: the handshake compares MAJOR.MINOR
(`rendererNetwork.cpp:512`), and a peer of another build is not detected (unchanged, recorded).

## `CRemoteICacheChannel` (Core, `render/remoteChannel.{h,cpp}`)

- `remoteChannel.h` keeps `class CIrradianceCache;` and the constructor; the definition comes from
  `texture/irradianceCache.h`.
- **`sendRemoteFrame`**:
  `cache->forEachNode([&](const CCacheSample *head) { count; if (n) { rcSend(n); for each: rcSend(s, sample,
  sizeof(CCacheSample), FALSE); } })`, then `rcSend(0)`. The order is the old stack order.
- **`recvRemoteFrame`**: for each batch, receive `n * sizeof(CCacheSample)` into a temporary buffer and set each
  `next = NULL`. Then, in arrival order, call `cache->insertReceived(sample)`, which copies the record into the
  cache's memory and places it by `dP` with no clamp, no `K` and no lock, as `remoteChannel.cpp:578-622`.
- **`sendSetupData`**: sends `cache->rootCenter()` and `rootSide()`.
- **`setup`**: receives them into locals, then `cache->setRoot(center, side)`.

## `CRemotePtCloudChannel` (Core)

- **`sendRemoteFrame`**: `cloud->getNumPoints()`, `cloud->points()`, `cloud->pointData()`, `cloud->dataSize`.
- **`recvRemoteFrame`**: unchanged (`cloud->store(...)`, already public).
- **`sendSetupData`**: `cloud->numChannels()` and `cloud->channelName(i)` replace `channelCount` and
  `channels[i].name`. The string built is the same.
- **`setup`**: `cloud->defineChannels(def)`, `cloud->setTransform(identity, identity)`, `cloud->setDPScale(1)`.

## Results the loopback test checks (T-A)

1. **Base and after**: the masked bytes the sender writes equal the recording (after: minus the key field).
2. **Base and after**: the receiving cache, written with `CACHE_WRITE` to a file, equals the recording byte for byte.
   That file records the whole tree (node centres, sides, records, child markers) and carries no key. The receiving
   point cloud's points, data and channel names equal the sender's.
3. **Finished tree only** (T-A2): the receiver's `lookupCached` at the sender's record positions, with a counting draw
   callback, against the sender's.
   - If the receiver's placement rule (by `dP`, without the sender's `K`) makes them differ, T-A2 records the
     difference and reports it. It is not fixed (OQ-5, approved 2026-11-07).
   - The base tree has no context-free lookup, so this part cannot be recorded there.
