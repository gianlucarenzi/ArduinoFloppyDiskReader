#!/bin/sh
USEMAKEFILE=$1

if [ "${USEMAKEFILE}" != "" ]; then
	echo "USING MAKEFILE"
	make clean
	make NONINTERACTIVE=-DUSEGUI -j8
	rval=$?
	if [ ${rval} -ne 0 ]; then
		echo "ERROR ON COMPILING WITH Makefile"
		exit 1
	fi
else
	echo "USING CMAKE"
	mkdir -p build
	cd build
	cmake ../
	make -j8
	rval=$?
	if [ ${rval} -ne 0 ]; then
		echo "ERROR ON COMPILING WITH CMAKE"
		exit 1
	fi
	# Copy it in the upper directory
	cp build/ArduinoFloppyReader/avrfloppyreader ../
	rm -rf build
	cd ..
fi

# Qt Interface Compilation
cd AVRFloppyReaderQt/avrfloppyQt
qmake
make clean
make -j8
# On POSIX the Qt application is looking for those files
touch /tmp/side
touch /tmp/track
touch /tmp/bad
./avrfloppyQt
