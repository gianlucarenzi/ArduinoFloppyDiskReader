#!/bin/bash
echo "*** $0 RUNNING XIMAGEVIEW ***"
export SDL_NOMOUSE=1
./ximageview vncsplash.png 0 &
echo "*** $0 XIMAGEVIEW ACTIVATED ***"
exit 0
