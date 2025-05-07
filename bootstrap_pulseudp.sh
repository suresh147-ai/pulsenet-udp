#!/usr/bin/env bash
set -e

mkdir -p build
cd build

cmake .. \
  -DCMAKE_INSTALL_PREFIX=../dist

cmake --build . --config Release
cmake --install . --config Release
