# Ground truth

What this project treats as authoritative, and the numbers and conventions
everything else is measured against. Nothing in this repository may contradict
this file without an explicit note explaining why.

## Source precedence

1. **The assignment specification** (`ECE-209-S25-CA1.pdf`, ECE 209AS, Spring 2025).
   Authoritative on the metric, the baseline, and the constraints.
2. **The CBP-2 infrastructure and its `doc/index.html`** (2006). Authoritative on
   the predictor interface and trace format. Silent on storage budget.
3. **The L-TAGE literature** (Seznec & Michaud). Authoritative on predictor design.

Where these disagree, the higher entry wins. Where all are silent, we decide and
record the decision here.

## The baseline

The specification publishes the expected output of the stock CBP-2 gshare
predictor (32,768 entries, 15 bits of global history) across all 20 traces, and
states an average of **6.305 MPKI**.

**Reproduced 2026-08-11.** All 20 per-trace values match the published table
exactly. See `results/stock-gshare.csv`, which records the commit that produced
it. Exact mean: **6.30595**.

Every accuracy claim in this repository is a delta against this number, measured
by the same harness on the same traces.

## The metric

- **MPKI** = `1000 * conditional_direction_mispredictions / 1e8`, computed by the
  framework. Each trace is *defined* as exactly 100M instructions; this is not
  measured against a real instruction count.
- **The reported figure is the plain unweighted mean** of the 20 per-trace MPKI
  values. Not instruction-weighted, not geometric.
- **Truncation, not rounding.** The specification's `run` script divides with `dc`
  at `scale=3`, which truncates. `tools/sweep.sh` matches this so our headline
  number is directly comparable, and additionally records `average_exact` at five
  decimal places so precision is never silently lost.
- Only conditional branches are scored, but `predict()` and `update()` are called
  on every branch, including calls, returns, and indirects.

## Storage

The specification states there is **no storage constraint** for this project, but
requires that the design's area overhead be explained and that tradeoffs be
realistic. Any budget in this repository is therefore **self-imposed** and must be
justified, not presented as an external requirement.

## Rules

- No accuracy figure appears anywhere in this repository — README, docs, commit
  messages, report — unless a CSV under `results/` produced it.
- Every CSV records the commit hash and whether the tree was clean. A measurement
  taken against a dirty tree is marked `DIRTY` and is not citable.
- Configurations are compared only against runs from the same harness. Published
  numbers from other papers are context, never baselines.
