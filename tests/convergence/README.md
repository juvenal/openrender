# Convergence harness (spec 039)

Checks that each sampler backend converges toward a high-sample-count
reference as the sample count grows (FR-033, SC-003b).

Status: skeleton only (spec 039 Phase 1). `test_convergence` is an empty
executable and no test is registered. The harness, the scene templates and the
`convergence-reference` target arrive in Phase 8.

- `reference/` holds the checked-in reference renders (research §M4 records
  what generating them costs).
- The `convergence` label is opt-in: it is never part of the default `ctest`
  run. Use `ctest --test-dir build -L convergence`.
