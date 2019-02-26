#!/bin/bash

DELAY=$1

S=0

for track in `seq 81`;
do
	echo -n $track > /tmp/track
	if [ $S -eq 1 ]; then
		echo -n "Lower" > /tmp/side
		S=0
	else
		echo -n "Upper" > /tmp/side
		S=1
	fi
	sleep $DELAY
done

echo -n "Lower" > /tmp/side
echo -n 0 > /tmp/track

exit 0
