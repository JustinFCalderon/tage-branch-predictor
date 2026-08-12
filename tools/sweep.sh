#!/usr/bin/env bash
# Run the predictor over all CBP-2 traces and record results with provenance.
# Usage: tools/sweep.sh [predict-binary] [traces-dir] [label]
set -euo pipefail

PREDICT="${1:-third_party/cbp2/src/predict}"
TRACES="${2:-third_party/cbp2/traces}"
LABEL="${3:-unlabeled}"

[ -x "$PREDICT" ] || { echo "error: no predict binary at $PREDICT" >&2; exit 1; }
[ -d "$TRACES" ]  || { echo "error: no traces dir at $TRACES" >&2; exit 1; }

mkdir -p results
OUT="results/${LABEL}.csv"

GITHASH=$(git rev-parse --short HEAD 2>/dev/null || echo no-git)
git diff --quiet 2>/dev/null && TREE=clean || TREE=DIRTY

{
  echo "# label,${LABEL}"
  echo "# date,$(date -u +%Y-%m-%dT%H:%M:%SZ)"
  echo "# git,${GITHASH},${TREE}"
  echo "# binary,${PREDICT}"
  echo "trace,mpki"
} > "$OUT"

for f in $(find "$TRACES" -name '*.trace.*' | sort); do
  name=$(basename "$(dirname "$f")")
  mpki=$("$PREDICT" "$f" | tail -1 | sed 's/ MPKI//')
  printf "%-16s %8s\n" "$name" "$mpki"
  echo "${name},${mpki}" >> "$OUT"
done

# The assignment's `run` script averages with dc at scale=3, which TRUNCATES;
# awk's printf rounds. Match the spec's convention so our headline number is
# directly comparable to the leaderboard, but keep full precision in the file.
exact=$(awk -F, '!/^#/ && $1!="trace" {s+=$2; n++} END {printf "%.5f", s/n}' "$OUT")
avg=$(awk -F, '!/^#/ && $1!="trace" {s+=$2; n++} END {printf "%.3f", int(s/n*1000)/1000}' "$OUT")
echo "average,${avg}" >> "$OUT"
echo "average_exact,${exact}" >> "$OUT"
printf "\n%-16s %8s   (exact %s)\n" "AVERAGE" "$avg" "$exact"
echo "wrote $OUT"
