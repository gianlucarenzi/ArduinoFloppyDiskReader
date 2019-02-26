#!/bin/sh
make clean
make NONINTERACTIVE=-DUSEGUI -j8
cd AVRFloppyReaderQt/avrfloppyQt
qmake
make clean
make -j8
touch /tmp/side
touch /tmp/track
touch /tmp/bad
./avrfloppyQt
