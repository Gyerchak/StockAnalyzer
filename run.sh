#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
if [ ! -d build ]; then
  cmake -B build -DCMAKE_BUILD_TYPE=Release
fi
cmake --build build -j
cd build
exec ./stockanalyzer "$@"
