#!/bin/sh

set -e

. fsbuild/system.sh

if [ $SYSTEM_OS = "Windows" ]; then
./configure --enable-static
else
./configure
fi
