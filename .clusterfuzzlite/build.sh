#!/bin/bash -eu

ROOT="${SRC:-$(pwd)}"
BUILD_DIR="$ROOT/build-cfl"
mkdir -p "$BUILD_DIR" "$OUT"
cd "$ROOT"

COMMON_SRCS=(
  src/helioforge/byte_tools.cc
  src/helioforge/crosscheck.cc
  src/helioforge/schema.cc
  src/binpack/binpack.cc
  src/binpack/manifest.cc
  src/minidb/minidb.cc
  src/minidb/query.cc
  src/cfgscript/cfgscript.cc
  src/cfgscript/evaluator.cc
  src/streamcodec/streamcodec.cc
  src/streamcodec/session.cc
)

for target in binpack minidb cfgscript streamcodec; do
  "$CXX" $CXXFLAGS -std=c++17 -Iinclude \
    "fuzz/${target}_fuzzer.cc" "${COMMON_SRCS[@]}" \
    $LIB_FUZZING_ENGINE -o "$OUT/${target}_fuzzer"
done
