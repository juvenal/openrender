---
title: "Previous Releases"
date: 2025-12-08
---

# Previous Releases

## Development build (2026-08-27)

- Fixed a `.rslo` interpreter crash triggered by comparing a varying-indexed
  element of a `uniform string` array against a string, e.g. `usarr[findex]
  == "a"` where `findex` is a varying value (such as a loop counter or a
  per-point computed index). Shaders using this pattern used to crash; they
  now render correctly under both the interpreter (`.rslo`) and JIT (`.slo`)
  backends.
- Investigated the LLVM JIT (`.slo`) shading backend's per-instruction call
  overhead for computations that only need to run once per shader invocation
  rather than once per shading point. A targeted fix was implemented and
  independently verified as functioning correctly, but controlled
  measurement found it produced no measurable wall-clock improvement on any
  tested shader — JIT-rendered shaders continue to run at roughly the same
  speed relative to the interpreter as before (about 1.05x-1.4x depending on
  the shader, unchanged within measurement noise). JIT shading correctness
  is unaffected either way.

**openRender 2.1.1 is out**!  New features include:

- [Conditional RIB](/openrender/manual/reference/conditional-rib/) - allowing a single RIB file to be reused with optional sections
- [RIB Resources](/openrender/manual/reference/rib-resources/) - allows
- [Ptc API](/openrender/manual/reference/ptc-api/) - allows reading and writing of openRender point clouds
- [User Attributes And Options](/openrender/manual/reference/user-attributes-and-options/) - passing custom attributes and options into shaders

Download: [SourceForge Files](http://sourceforge.net/project/showfiles.php?group_id=59462&package_id=55537&release_id=497138), Release Notes: [SourceForge Notes](http://sourceforge.net/project/shownotes.php?release_id=497138&group_id=59462)

## 2.0.2

**openRender 2.0.2 is finally here**!  Some of the new features include:

- [Multithreading](/openrender/manual/reference/multithreading/) - optimal use of multiple processors
- Improved high quality ray tracing
- Many fixes and improvements
- 64-Bit clean codebase

Download: [SourceForge Files](http://sourceforge.net/project/showfiles.php?group_id=59462&package_id=55537&release_id=487701), Release Notes: [SourceForge Notes](http://sourceforge.net/project/shownotes.php?group_id=59462&release_id=487701)