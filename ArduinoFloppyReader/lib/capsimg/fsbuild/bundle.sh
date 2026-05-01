#!/bin/sh

set -e

. fsbuild/plugin.pre.sh

mkdir -p $PLUGIN_BINDIR
cp capsimg$SYSTEM_DLL $PLUGIN_BINDIR

mkdir -p $PLUGIN_READMEDIR
cp README.md $PLUGIN_READMEDIR/ReadMe.txt

mkdir -p $PLUGIN_LICENSESDIR
cp LICENCE.txt $PLUGIN_LICENSESDIR/CAPSImg.txt

if [ $SYSTEM_OS = "macOS" ]; then
cp $PLUGIN_BINDIR/capsimg.so $PLUGIN_BINDIR/capsimg.dylib
fi

. fsbuild/plugin.post.sh
