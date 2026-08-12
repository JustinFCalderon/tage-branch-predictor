# L-TAGE Branch Predictor — rebuild

A tagged-geometric-history branch predictor built against the CBP-2 framework,
rebuilt from the published design with a measured baseline and per-milestone tests.

## Status

**No results exist yet.** This README will carry a measured MPKI table once the
validation gate (TAGE core must beat the from-scratch gshare baseline on all 20
CBP-2 traces) has been passed. Until then there are deliberately no numbers here.

An earlier implementation of this predictor, written for a course, produced a
results table I could not reproduce from its source. This repository is the
rebuild: every figure it eventually reports is traceable to a run committed
under `results/`.

## Layout

- `src/` — predictor implementations
- `tests/` — standalone correctness tests
- `results/` — measurement CSVs (the evidence)
- `docs/` — design notes
- `third_party/cbp2/` — vendored CBP-2 infrastructure (2006), unmodified

## Framework

CBP-2 (2nd Championship Branch Prediction) infrastructure, released 2006.
Vendored unmodified under `third_party/cbp2/`. The predictor interface is two
virtual methods, `predict(branch_info&)` and `update(branch_update*, bool, unsigned)`.
Each trace is defined as exactly 100M instructions; MPKI is computed as
`1000 * direction_mispredictions / 1e8`.

## Build

    cd third_party/cbp2/src && make
    ./predict ../traces/256.bzip2/bzip2.trace.bz2
