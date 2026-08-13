#!/usr/bin/env bash
# Assemble a predict binary from the vendored framework plus our predictor.
#
# predict.cc resolves #include "my_predictor.h" relative to its own directory,
# so the only way to substitute ours without editing the vendored tree is to
# stage both into a build directory. third_party/ stays pristine.
set -euo pipefail
cd "$(dirname "$0")/.."

rm -rf build && mkdir -p build
cp third_party/cbp2/src/predict.cc \
   third_party/cbp2/src/trace.cc \
   third_party/cbp2/src/trace.h \
   third_party/cbp2/src/branch.h \
   third_party/cbp2/src/predictor.h \
   build/
cp src/*.h build/

# -Wno-write-strings and -Wno-unused-result silence the 2006 framework only;
# our headers are compiled under -Werror by tests/run_tests.sh.
g++ -std=c++17 -O2 -g -Wall -Wextra \
    -Wno-write-strings -Wno-unused-result \
    -o build/predict build/predict.cc build/trace.cc

echo "built build/predict"