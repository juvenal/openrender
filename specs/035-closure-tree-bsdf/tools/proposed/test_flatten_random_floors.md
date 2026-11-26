# Proposed change to an approved test: the population floors of `Closure_Flatten_Random` (a PAUSE)

`tests/closures/test_flatten.cpp`, end of `sectionRandom()`:

```cpp
CL_CHECK(totalLobes > 3000); // the random trees are not trivially empty
CL_CHECK(nonEmpty > 900);
```

## What happened

Every per-tree comparison of the 1000 random trees passes (the flattened list, the generic evaluator and the independent reference agree bit for bit, in order, weight and block). Only these two floors fail. They are not numerical tolerances: they guard against a degenerate generator (a population of mostly empty trees would make the bit-for-bit comparison vacuous). I wrote them as estimates before measuring.

Measured (a one-off print, since reverted; the keyed generator is deterministic): **1000 trees, 1865 lobes, 895 non-empty trees**. Expected from the generator: depth 2 to 6, a leaf with probability 1/3 at each level, a 12 % chance of a zero-weight component and a 10 % chance of a zero multiply, so about one tree in ten is empty and the mean is under two lobes per tree.

## Options

1. **Set the floors from the measurement**: `totalLobes > 1500` and `nonEmpty > 800` (80 % of the measured lobes; a floor 95 trees below the measured count). The guard keeps its purpose: a generator that returned mostly empty trees (nonEmpty near 100) or single lobes would still fail.
2. **Keep the floors and enrich the generator** so the population meets them: a leaf probability of 1/5 at depth > 0 and depth 3 to 6. This changes every tree the test builds, so the bit-for-bit evidence is re-collected on a different population.

I recommend 1: it changes two numbers that were guessed, not the population the equality checks run over. It lowers a floor, so I have not made it: the test stays red until you decide.

## Decision (maintainer, 2026-11-24)

Option 2: the generator is enriched, the floors stay (`totalLobes > 3000`, `nonEmpty > 900`). Done in `tests/closures/test_flatten.cpp`; the mutation runs that show the floors still catch a degenerate generator are in `validation.md`.
