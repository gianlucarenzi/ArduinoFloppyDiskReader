#!/bin/sh
# build_all.sh - run linux build and mingw cross-build in sequence
set -e
cd "$(dirname "$0")"

echo "clean"
make distclean

echo "2) Linux native: build"
make CROSS_COMPILE= all
make CROSS_COMPILE= clean

echo "4) Windows cross (mingw): build"
make CROSS_COMPILE=x86_64-w64-mingw32- all
make CROSS_COMPILE=x86_64-w64-mingw32- clean

echo "All builds completed."
