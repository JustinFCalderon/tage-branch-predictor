#!/usr/bin/env bash
# Build and run every test. Exit non-zero if any fails.
set -uo pipefail
cd "$(dirname "$0")"

CXXBIN=${CXX:-g++}
FLAGS="-std=c++17 -O2 -g -Wall -Wextra -Werror -fsanitize=address,undefined"
TESTS="folded_history_test storage_test tag_uniqueness_test lookup_test"

fail=0
for t in $TESTS; do
  printf '=== building %s ===\n' "$t"
  if ! $CXXBIN $FLAGS -o "$t" "$t.cc"; then
    echo "BUILD FAILED: $t"
    fail=1
    continue
  fi
  printf '=== running  %s ===\n' "$t"
  if ! ./"$t"; then
    fail=1
  fi
done

if [ "$fail" -ne 0 ]; then
  echo
  echo "TESTS FAILED"
else
  echo
  echo "all tests passed"
fi
exit $fail
